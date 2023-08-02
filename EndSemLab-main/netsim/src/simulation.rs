use std::{
    net::{TcpStream, IpAddr},
    io::{Read, Write, ErrorKind},
    thread::{self, JoinHandle}, time::Duration,
    sync::{Arc, Mutex, mpsc::{self, Receiver, Sender}}, collections::HashMap,
};

use crate::{
    error::ErrorHandler, expect_result_or_crash, crash, protocol::{RaniHeader, RaniPacket, validate_packets},
    tests::{Tester, TestCase},
    BASE_LINK_PORT, err_return,
};

/**************************
 * CONSTANTS
***************************/

const LINK_READ_TIMEOUT: Duration = Duration::from_millis(100);

/**************************
 * STRUCTURES
***************************/

#[derive(Debug)]
enum Message {
    /// The `Option<u8>` is a custom checksum value.
    Active(Result<(Arc<RaniPacket>, Option<u8>), String>),
    Inactive,
    NoResponse(RaniPacket),
    End,
}

/**************************
 * FUNCTIONS
***************************/

fn link_handler(
    remote_ip: IpAddr,
    link_id: usize,
    mut stream: TcpStream,
    rx: Receiver<Message>,
    tx: Sender<Message>,
) -> Result<(), ()> {
    err_return!(stream.set_read_timeout(Some(LINK_READ_TIMEOUT)), "stream read timeout");
    let id = format!(
        "{}:{}",
        err_return!(stream.peer_addr(), "stream peer addr").ip(),
        err_return!(stream.local_addr(), "stream local addr").port() % BASE_LINK_PORT,
    );

    loop {
        let message = err_return!(rx.recv(), "link channel receive failed");
        match message {
            Message::Active(Ok((pkt, custom_checksum))) => {
                let mut buf = vec![0u8; pkt.size()];
                err_return!(pkt.serialise(&mut buf), "link packet serialisation");
                if let Some(custom_checksum) = custom_checksum {
                    buf[6] = custom_checksum;
                }

                err_return!(stream.write_all(&buf), "link stream write");
            },
            Message::Inactive => (),
            Message::End => break,
            Message::NoResponse(pkt) => {
                let mut buf = vec![0u8; pkt.size()];
                err_return!(pkt.serialise(&mut buf), "link packet serialisation");
                err_return!(stream.write_all(&buf), "link stream write");
                err_return!(stream.flush(), "link stream flush");

                // No waiting for response after sending ERR packet.
                continue;
            },
            _ => {
                eprintln!("[!] invalid message from main thread");
                return Err(());
            },
        }

        let mut buf = [0u8; 256];
        let message = match stream.read(&mut buf) {
            Ok(bytes_read) if bytes_read == 0 => Message::Active(Err(format!("{id}: Connection closed at user"))),

            Ok(bytes_read) => match RaniPacket::deserialise(&buf[..bytes_read]) {
                Ok((pkt, _)) => Message::Active(Ok((Arc::new(pkt), None))),
                Err(e) => Message::Active(Err(format!("link {id}: {}", e))),
            },

            Err(e) if e.kind() == ErrorKind::WouldBlock => Message::Inactive,

            Err(e) => {
                eprintln!("[!] link stream read failed: {}", e);
                return Err(());
            },
        };

        err_return!(tx.send(message), "link channel write");
    }

    eprintln!("[*] link <{}:{}> ended", remote_ip, link_id);
    Ok(())
}

/**
 * main simulator thread spawns n threads for handling n links.
 * there is two way communication between the main thread and each of the link threads
 * at the start of each packet cycle, each link thread is in an idle state. 
 * the main thread sends an active signal and a packet to the link thread that is supposed to send the packet, 
 and an inactive signal to the others.
 * all threads now block on their links (with a timeout), waiting for a possible packet.
 * as soon as a thread gets a packet, it sends the packet back to the main thread.
 * The main thread validates the link and the packet and sends error message if any.
 * The main thread also sends an error message if no link receives a packet after the timeout.
 * If all is well, cycle begins again.
 */
fn simulation_cycle(
    remote_ip: IpAddr,
    channels: &mut Vec<(Sender<Message>, Receiver<Message>)>,
    error_handler: Arc<Mutex<ErrorHandler>>,
    tester: Arc<Tester>,
) -> Result<(), ()> {
    // Some(String) if error occured else None.
    let mut errors: HashMap<usize, Option<String>> = HashMap::new();
    let mut error_info = Vec::new();

    for (test_id, test) in tester.cloned_iter().enumerate() {
        let TestCase {
            send_link,
            send_packet,
            custom_checksum,
            recv_links,
            recv_packets,
            msg,
        } = test;
        error_info.push(msg);

        // send using channel communication.
        for (i, (tx, _)) in channels.iter_mut().enumerate() {
            let message = if i == send_link {
                Message::Active(Ok((send_packet.clone(), custom_checksum)))
            } else {
                Message::Inactive
            };

            err_return!(tx.send(message), "<{remote_ip}> main thread channel write");
        }

        // recv using channel communication.
        let mut received = false;
        let mut received_count = 0;
        let mut has_error_occured = false;
        for (i, (_, rx)) in channels.iter_mut().enumerate() {
            let message = err_return!(rx.recv(), "<{remote_ip}> main thread channel read");
            if recv_links.is_some() && recv_links.as_ref().unwrap().contains(&i) {
                let (idx, _) = recv_links.as_ref().unwrap().iter().enumerate().find(|(_, &e)| i == e).unwrap();
                let recv_packet = &recv_packets.as_ref().expect("invalid test case")[idx];
                match message {
                    Message::Active(Ok((pkt, _))) => {
                        received = true;
                        received_count += 1;

                        // Validate that packet is correct or send error.
                        if let Err(e) = validate_packets(&pkt, &recv_packet) {
                            has_error_occured = true;
                            eprintln!("[*] <{remote_ip}> Error: {e}");
                            errors.insert(test_id, Some(e));
                        }
                    },

                    Message::Active(Err(e)) => {
                        received = true;
                        has_error_occured = true;
                        eprintln!("[*] <{remote_ip}> Error: {}", e);
                        errors.insert(test_id, Some(e));
                    },

                    Message::Inactive => {
                        has_error_occured = true;
                        eprintln!("[*] Error: <{remote_ip}:{i}> Did not receive packet on link");
                    },

                    _ => {
                        eprintln!("[!] invalid message from link thread");
                        return Err(());
                    },
                }
            } else {
                match message {
                    Message::Inactive => (),

                    Message::Active(Ok(..)) => {
                        received = true;
                        has_error_occured = true;
                        eprintln!("[*] <{remote_ip}> Error: Packet routed incorrectly to link {i}");
                        errors.insert(test_id, Some("Packet routed incorrectly".to_string()));
                    },

                    Message::Active(Err(msg)) => {
                        has_error_occured = true;
                        eprintln!("[*] <{remote_ip}> Error: {}", msg);
                        errors.insert(test_id, Some(msg));
                    }

                    _ => {
                        eprintln!("[!] invalid message from link thread");
                        return Err(());
                    },
                }
            }
        }
        if !received && recv_links.is_some() {
            has_error_occured = true;
            let msg = "Packet not received on any link";
            eprintln!("[*] <{remote_ip}> Error: {msg}");
            errors.insert(test_id, Some(msg.to_string()));
        }
        else if received && recv_links.is_some() && received_count != recv_links.as_ref().unwrap().len() {
            has_error_occured = true;
            let msg = "Packets not received on all required links";
            eprintln!("[*] <{remote_ip}> Error: {msg}");
            errors.insert(test_id, Some(msg.to_string()));
        }

        // Nothing has been pushed to the test case results.
        if !has_error_occured {
            errors.insert(test_id, None);
        }
    }

    // Format all error messages.
    assert_eq!(errors.len(), error_info.len());
    let mut has_error_ever_occured = false;
    let mut error_msg = String::with_capacity(128);
    for test_id in 0..errors.len() {
        let test_msg = match errors.remove(&test_id).expect("No result added for test id") {
            Some(msg) => {
                has_error_ever_occured = true;
                format!("FAILED \'{}\'", msg)
            },
            None => "PASSED ".to_string(),
        };
        error_msg.push_str(&format!("Routing Test {} [{}] : {}\n", test_id + 1, error_info[test_id], test_msg));
    }
    
    if let Err(e) = expect_result_or_crash!(error_handler.lock(), "error handler lock")
            .send_message(remote_ip, error_msg) {
            eprintln!("[!] Couldn't send error message to {}: {}", remote_ip, e);
        }

    // Send ERR to all links if error has occured, else send END to all links.
    for (tx, _) in channels.iter_mut() {
        let last_pkt = if has_error_ever_occured { error_packet() } else { end_packet() };
        err_return!(tx.send(Message::NoResponse(last_pkt)), "<{remote_ip}> main thread channel write");
    }

    // Send exit signal to all link threads.
    for (tx, _) in channels.iter_mut() {
        err_return!(tx.send(Message::End), "<{remote_ip}> main thread channel write");
    }

    if !has_error_ever_occured {
        eprintln!("[*] Simulation successful for {}", remote_ip);
    }

    Ok(())
}

pub fn router_simulation(
    remote_ip: IpAddr,
    mut links: Vec<TcpStream>,
    error_handler: Arc<Mutex<ErrorHandler>>,
    tester: Arc<Tester>,
) -> Result<(), ()> {
    validate_links(remote_ip, &mut links)?;

    eprintln!("[*] Router links established at {}", remote_ip);

    let mut handles = Vec::with_capacity(links.len());
    let mut channels: Vec<(Sender<Message>, Receiver<Message>)> = Vec::with_capacity(links.len());

    for (link_id, stream) in links.into_iter().enumerate() {
        let (main_tx, link_rx): (Sender<Message>, Receiver<Message>) = mpsc::channel();
        let (link_tx, main_rx): (Sender<Message>, Receiver<Message>) = mpsc::channel();

        let handle = match thread::Builder::new()
            .name(format!("link thread {}:{}", link_id, remote_ip))
            .spawn(move || link_handler(remote_ip, link_id, stream, link_rx, link_tx)) {
            Ok(handle) => handle,
            Err(e) => {
                eprintln!("[!] <{}> link thread creation failed: {}", remote_ip, e);
                cleanup_links(handles, channels);
                return Err(());
            },
        };

        handles.push(handle);
        channels.push((main_tx, main_rx));
    }
    
    let result = simulation_cycle(remote_ip, &mut channels, error_handler, tester);

    cleanup_links(handles, channels);
    eprintln!("[*] Simulation ended");

    result
}

fn cleanup_links(mut handles: Vec<JoinHandle<Result<(), ()>>>, mut channels: Vec<(Sender<Message>, Receiver<Message>)>) {
    let mut i = 0;
    loop {
        if handles.is_empty() {
            break;
        }
        if i >= handles.len() {
            i = 0;
        }

        if handles[i].is_finished() {
            let handle = handles.swap_remove(i);
            channels.swap_remove(i);
            if let Err(..) = handle.join() {
                // Some thread error'ed, send exit signal to all other threads.
                for (tx, _) in channels.iter() {
                    if let Err(..) = tx.send(Message::End) {};
                }
            }
        }
        i += 1;
    }
}

fn validate_links(remote_ip: IpAddr, links: &mut Vec<TcpStream>) -> Result<(), ()>  {
    for (i, link) in links.iter_mut().enumerate() {
        let mut buf = [0; 1];
        err_return!(link.read_exact(&mut buf), "link ID byte read");
        if i != buf[0] as usize {
            eprintln!("[!] <{}:{}> invalid link ID byte", remote_ip, i);
            return Err(());
        }
    }
    Ok(())
}

pub fn error_packet() -> RaniPacket {
    let mut header = RaniHeader::new(0, 0, 0, 15, 0);
    header.set_err();

    RaniPacket::new(
        header,
        crate::protocol::RaniPayload::Data(Vec::new()),
    )
}

pub fn end_packet() -> RaniPacket {
    let mut header = RaniHeader::new(0, 0, 0, 15, 0);
    header.set_end();

    RaniPacket::new(
        header,
        crate::protocol::RaniPayload::Data(Vec::new()),
    )
}
use std::{
    net::{TcpListener, TcpStream, SocketAddr, Ipv4Addr, IpAddr},
    thread, sync::{Arc, Mutex}, collections::HashMap, str::FromStr,
};

use tests::Tester;

use crate::error::ErrorHandler;

mod error;
mod protocol;
mod simulation;
mod tests;

/// IP -> (count of connections connected, array of connections).
type ConnectionTable = HashMap::<IpAddr, (usize, Vec<Option<TcpStream>>)>;

/**************************
 * CONSTANTS
***************************/

const BASE_LINK_PORT: u16 = 10000;
const ERROR_MSG_PORT: u16 = 22222;

/// Should fit into a `u16`.
const ROUTER_LINK_COUNT: usize = 4;

/**************************
 * FUNCTIONS
***************************/

fn link_listen_thread(
    listener: TcpListener,
    router_link: usize,
    connections: Arc<Mutex<ConnectionTable>>,
    error_handler: Arc<Mutex<ErrorHandler>>,
    tester: Arc<Tester>,
) {
    eprintln!("[*] Listening for router link {}", router_link);

    loop {
        let (stream, addr) = match listener.accept() {
            Ok(result) => result,
            Err(e) => {
                eprintln!("[*] WARN: accept failed: {}", e);
                continue;
            },
        };

        eprintln!("[*] Accepted from {} at router link {}", addr.ip(), router_link);

        // Add accepted stream to connection table.
        let mut connections = expect_result_or_crash!(connections.lock(), "connections mutex lock");
        let (conn_count, conn_vec) = connections.entry(addr.ip()).or_insert_with(|| {
            let mut vec = Vec::with_capacity(ROUTER_LINK_COUNT);
            for _ in 0..ROUTER_LINK_COUNT { vec.push(None); }
            (0, vec)
        });
        conn_vec[router_link] = Some(stream);
        *conn_count += 1;

        // If all router links have connected, start simulation.
        if *conn_count == ROUTER_LINK_COUNT {
            let (ip, (_, links)) = match connections.remove_entry(&addr.ip()) {
                Some(v) => v,
                None => crash!("connection should have been in table"),
            };

            let error_handler = error_handler.clone();
            let tester = tester.clone();

            thread::Builder::new()
                .name(format!("simulation thread {}", ip))
                .spawn(move || {
                    let links: Vec<TcpStream> = links.into_iter()
                        .map(|e| e.expect("link stream does not exist")).collect();
                    simulation::router_simulation(ip, links, error_handler, tester)
                })
                .expect("thread creation failed");
        }
    }
}

fn main() {
    let mut args = std::env::args();
    args.next();
    let ip = args.next().expect("Expecting IP address as the first argument");
    let ip = IpAddr::V4(Ipv4Addr::from_str(&ip).expect("Invalid IPv4 address"));

    let tests_file = std::fs::read("./tests.json").expect("tests.json read failed");
    let tester = Arc::new(Tester::parse(&tests_file));

    // Spawn error port listener.
    let error_handler = Arc::new(Mutex::new(ErrorHandler::new()));

    let addr = SocketAddr::new(ip, ERROR_MSG_PORT);
    let listener = TcpListener::bind(addr).expect("bind failed");
    let error_handler_clone = error_handler.clone();
    thread::Builder::new()
        .name("error listen thread".to_string())
        .spawn(move || error::error_listen_thread(listener, error_handler_clone))
        .expect("thread creation failed");


    let connections = Arc::new(Mutex::new(ConnectionTable::new()));

    for link in 0..ROUTER_LINK_COUNT {
        let addr = SocketAddr::new(ip, BASE_LINK_PORT + link as u16);
        let listener = TcpListener::bind(addr).expect("bind failed");
        let connections = connections.clone();
        let error_handler = error_handler.clone();
        let tester = tester.clone();

        // Spawn a listener thread for each link port.
        thread::Builder::new()
            .name(format!("link listen thread {}", link))
            .spawn(move || link_listen_thread(listener, link, connections, error_handler, tester))
            .expect("thread creation failed");
    }

    loop {}
}
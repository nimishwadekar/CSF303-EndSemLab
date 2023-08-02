use std::sync::Arc;

use serde_json::{self, Value, Map};

use crate::protocol::{RaniPacket, RaniHeader, RaniPayload, RaniCommandPayload, RaniCommandEntry};

/**************************
 * CONSTANTS
***************************/


/**************************
 * TRAITS
***************************/

trait Extract {
    fn extract_u64(&mut self, key: &str) -> u64;
    fn extract_optional_u64(&mut self, key: &str) -> Option<u64>;
    fn extract_str(&mut self, key: &str) -> String;
    fn extract_arr(&mut self, key: &str) -> Vec<Value>;
    fn extract_obj(&mut self, key: &str) -> Map<String, Value>;
}

trait BoundsCheckConvert {
    fn to_u8(self) -> u8;
    fn to_u16(self) -> u16;
    fn to_u32(self) -> u32;
    fn to_usize(self) -> usize;
}

/**************************
 * STRUCTURES
***************************/

#[derive(Debug, Clone)]
pub struct TestCase {
    pub send_link: usize,
    pub send_packet: Arc<RaniPacket>,
    pub custom_checksum: Option<u8>,
    pub recv_links: Option<Vec<usize>>,
    pub recv_packets: Option<Arc<Vec<RaniPacket>>>,
    pub msg: String,
}

pub struct Tester {
    tests: Vec<TestCase>,
}

pub struct TesterClonedIterator<'a> {
    tester: &'a Tester,
    index: usize,
}

/******************************************
 * STANDARD LIBRARY TRAIT IMPLEMENTATIONS
*******************************************/

impl From<Map<String, Value>> for TestCase {
    fn from(mut object: Map<String, Value>) -> Self {
        let send_link = object.extract_u64("send_link").to_usize();
        let send_packet = object.extract_obj("send_packet");
        let msg = object.extract_str("msg");

        let custom_checksum = match object.extract_optional_u64("checksum") {
            Some(checksum) => Some(checksum.to_u8()),
            None => None,
        };

        let recv_links: Vec<usize> = match object.remove("recv_link") {
            None => {
                assert!(object.is_empty());
                return Self {
                    send_link,
                    send_packet: Arc::new(send_packet.into()),
                    custom_checksum,
                    recv_links: None,
                    recv_packets: None,
                    msg,
                }
            },
            Some(v) if v.is_u64() => vec![v.as_u64().unwrap().to_usize()],
            Some(v) if v.is_string() && v.as_str().unwrap() == "all" => (0..crate::ROUTER_LINK_COUNT).collect(),
            _ => panic!("u64 or \"all\" recv_link expected"),
        };

        let mut recv_packets = match object.remove("recv_packet") {
            Some(Value::Object(v)) => vec![v],
            Some(Value::Array(v)) => v.into_iter().map(|val| match val {
                Value::Object(obj) => obj,
                _ => panic!("Expecting packet object in recv_packet"),
            }).collect(),
            _ => panic!("Expecting packet object or array of packet objects in recv_packet"),
        };
        assert_eq!(recv_links.len(), recv_packets.len());
        assert!(object.is_empty());

        // Add all key-value pairs from `send_packet` into `recv_packet` if they don't exist.
        for recv_packet in recv_packets.iter_mut() {
            for (key, value) in send_packet.iter() {
                recv_packet.entry(key).or_insert(value.clone());
            }
        }

        let send_packet = Arc::new(send_packet.into());
        let recv_links = Some(recv_links);
        let recv_packets = Some(Arc::new(recv_packets.into_iter().map(|e| e.into()).collect()));
        
        Self { send_link, send_packet, custom_checksum, recv_links, recv_packets, msg }
    }
}

impl From<Map<String, Value>> for RaniPacket {
    fn from(mut object: Map<String, Value>) -> Self {
        let src = object.extract_u64("src").to_u8();
        let dest = object.extract_u64("dest").to_u8();
        let ttl = object.extract_u64("ttl").to_u8();
        let seq_no = object.extract_u64("seq_no").to_u16();

        let flags = object.extract_arr("flags");
        let mut flag_err = false;
        let mut flag_end = false;
        let mut flag_ack = false;
        for flag in flags {
            match flag {
                Value::String(ref flag) if flag == "ERR" => flag_err = true,
                Value::String(ref flag) if flag == "END" => flag_end = true,
                Value::String(ref flag) if flag == "ACK" => flag_ack = true,
                Value::String(ref flag) => panic!("invalid flag: {}", flag),
                _ => panic!("flags can only be strings"),
            }
        }
        
        let payload = object.remove("payload").expect("payload not found");

        let payload = match object.extract_str("type").as_str() {
            "cmd" => {
                match payload {
                    Value::Object(payload) => RaniPayload::Command(payload.into()),
                    _ => panic!("cmd packets only allow object payloads"),
                }
            },

            "data" => {
                match payload {
                    Value::String(payload) => RaniPayload::Data(payload.as_bytes().to_vec()),

                    Value::Array(payload) => {
                        let payload: Vec<u8> = payload.into_iter().map(|e| {
                            e.as_u64().expect("data payload array elements should be u8").to_u8()
                        }).collect();
                        RaniPayload::Data(payload)
                    },

                    _ => panic!("data packets only allow string or byte array payloads"),
                }
            },

            s => panic!("invalid packet type: {s}"),
        };

        let payload_length = (payload.size() as u64).to_u8();
        let mut header = RaniHeader::new(src, dest, payload_length, ttl, seq_no);

        if flag_err { header.set_err(); }
        if flag_end { header.set_end(); }
        if flag_ack { header.set_ack(); }
        if let RaniPayload::Command(..) = payload { header.set_command(); }
        
        RaniPacket::new(header, payload)
    }
}

impl From<Map<String, Value>> for RaniCommandPayload {
    fn from(mut object: Map<String, Value>) -> Self {
        let entry_count = object.extract_u64("entry_count").to_u8();
        let timestamp = object.extract_u64("timestamp").to_u32();

        let entries: Vec<RaniCommandEntry> = object.extract_arr("entries").into_iter().map(|e| {
            match e {
                Value::Array(entry) 
                    if entry.len() == 2 &&
                    entry[0].is_u64() && 
                    entry[1].is_u64()
                => {
                    let dest = entry[0].as_u64().unwrap().to_u8();
                    let cost = entry[1].as_u64().unwrap().to_u8();
                    RaniCommandEntry::new(dest, cost)
                },
                _ => panic!("cmd table entries must be three-element u8 arrays"),
            }
        }).collect();

        RaniCommandPayload::new(entry_count, timestamp, entries)
    }
}

/**************************
 * TRAIT IMPLEMENTATIONS
***************************/

impl Extract for Map<String, Value> {
    fn extract_u64(&mut self, key: &str) -> u64 {
        self.remove(key).expect(&format!("`{key}` not found")).as_u64().expect(&format!("u64 {key} expected"))
    }

    fn extract_optional_u64(&mut self, key: &str) -> Option<u64> {
        Some(self.remove(key)?.as_u64().expect(&format!("u64 {key} expected")))
    }

    fn extract_str(&mut self, key: &str) -> String {
        match self.remove(key).expect(&format!("`{key}` not found")) {
            Value::String(s) => s,
            _ => panic!("string {key} expected"),
        }
    }

    fn extract_arr(&mut self, key: &str) -> Vec<Value> {
        match self.remove(key).expect(&format!("`{key}` not found")) {
            Value::Array(arr) => arr,
            _ => panic!("array {key} expected"),
        }
    }

    fn extract_obj(&mut self, key: &str) -> Map<String, Value> {
        match self.remove(key).expect(&format!("`{key}` not found")) {
            Value::Object(object) => object,
            _ => panic!("object {key} expected"),
        }
    }
}

impl BoundsCheckConvert for u64 {
    fn to_u8(self) -> u8 {
        if self > u8::MAX as u64 { panic!("{self} too big for u8") }
        else { self as u8 }
    }

    fn to_u16(self) -> u16 {
        if self > u16::MAX as u64 { panic!("{self} too big for u16") }
        else { self as u16 }
    }

    fn to_u32(self) -> u32 {
        if self > u32::MAX as u64 { panic!("{self} too big for u32") }
        else { self as u32 }
    }

    fn to_usize(self) -> usize {
        if self > usize::MAX as u64 { panic!("{self} too big for usize") }
        else { self as usize }
    }
}

impl<'a> Iterator for TesterClonedIterator<'a> {
    type Item = TestCase;

    fn next(&mut self) -> Option<Self::Item> {
        if self.index >= self.tester.tests.len() {
            return None
        }
        else {
            let item = self.tester.tests[self.index].clone();
            self.index += 1;
            Some(item)
        }
    }
}

/**************************
 * IMPLEMENTATIONS
***************************/

impl Tester {
    pub fn parse(tests_file: &[u8]) -> Self {
        let mut tester = Tester { tests: Vec::new() };
        let json: Vec<Map<String, Value>> = serde_json::from_slice(tests_file).expect("JSON deserialisation failed");
        
        for case in json {
            let case: TestCase = case.into();
            tester.tests.push(case);
        }

        tester
    }

    pub fn cloned_iter(&self) -> impl Iterator<Item = TestCase> + '_ {
        TesterClonedIterator { tester: self, index: 0 }
    }
}

/**************************
 * FUNCTIONS
***************************/


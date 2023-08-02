
/**************************
 * CONSTANTS
***************************/

const HEADER_SIZE: usize = 8;
const COMMAND_HEADER_SIZE: usize = 4;
const COMMAND_ENTRY_SIZE: usize = 2;

const FLAG_BIT_ERR: u8 = 4;
const FLAG_BIT_END: u8 = 5;
const FLAG_BIT_ACK: u8 = 6;

const TYPE_COMMAND: u8 = 4;
const TYPE_DATA: u8 = 8;

/**************************
 * STRUCTURES
***************************/

//  RaNi Header format:
//  (All unused fields are always zero.)
//
//  Offset      Size        Description
//  0           1           Source Address
//  1           1           Destination Address
//  2           1           Length of header + payload
//  3           1           Flags + TTL:
//                          Lower 4 bits (0-3) - TTL (4 bits)
//                          Bit 4 - ERR flag
//                          Bit 5 - END flag
//                          Bit 6 - ACK flag
//                          Bit 7 - unused
//  4           1           Upper 4 bits (4-7) - Type (command packet = 4, data packet = 8)
//                          Lower 4 bits (0-3) - Highest 4 bits (out of 12) of Seq. No.
//  5           1           Lower 8 bits (out of 12) of Seq. No.
//  6           1           8-bit 1's complement checksum of header + payload
//  7           1           Unused
//
//  Command packet payload format:
//  (Offset is from start of payload)
//
//  Offset      Size        Description
//  0           1           No. of DV Table entries in this packet
//  1           1           Upper 4 bits (4-7) - unused
//                          Lower 4 bits (0-3) - Highest 4 bits (out of 20) of timestamp (number of seconds since midnight)
//  2           2           Lower 16 bits (out of 20) of timestamp
//
//  Entry format:
//  5           1           Destination Address
//  6           1           Cost
//  Entry format continues for the rest of the packet. [Entry 2 is at offsets (7, 8), entry 3 at (9, 10), etc.]

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum RaniType {
    Command = TYPE_COMMAND,
    Data = TYPE_DATA,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub struct RaniHeader {
    src: u8,
    dest: u8,
    length: u8,
    ttl: u8,

    flag_err: bool,
    flag_end: bool,
    flag_ack: bool,
    packet_type: RaniType,

    seq_no: u16,
}

#[derive(Debug, PartialEq, Eq)]
pub struct RaniCommandEntry {
    dest: u8,
    cost: u8,
}

#[derive(Debug, PartialEq, Eq)]
pub struct RaniCommandPayload {
    entry_count: u8,
    timestamp: u32,
    entries: Vec<RaniCommandEntry>,
}

#[derive(Debug, PartialEq, Eq)]
pub enum RaniPayload {
    Command(RaniCommandPayload),
    Data(Vec<u8>),
}

#[derive(Debug, PartialEq, Eq)]
pub struct RaniPacket {
    header: RaniHeader,
    payload: RaniPayload,
}

/**************************
 * TRAIT IMPLEMENTATIONS
***************************/

impl TryFrom<u8> for RaniType {
    type Error = &'static str;
    
    fn try_from(value: u8) -> Result<Self, Self::Error> {
        match value {
            TYPE_COMMAND => Ok(RaniType::Command),
            TYPE_DATA => Ok(RaniType::Data),
            _ => Err("invalid RaNi packet type"),
        }
    }
}

/**************************
 * IMPLEMENTATIONS
***************************/

impl RaniHeader {
    pub fn new(
        src: u8,
        dest: u8,
        payload_length: u8,
        ttl: u8,
        seq_no: u16,
    ) -> Self {
        assert!(ttl < (1 << 4));
        assert!(seq_no < (1 << 12));
        Self {
            src,
            dest,
            length: HEADER_SIZE as u8 + payload_length,
            ttl,
            flag_err: false,
            flag_end: false,
            flag_ack: false,
            packet_type: RaniType::Data,
            seq_no,
        }
    }

    pub fn size(&self) -> usize {
        HEADER_SIZE
    }

    pub fn set_err(&mut self) {
        self.flag_err = true;
    }

    pub fn set_end(&mut self) {
        self.flag_end = true;
    }

    pub fn set_ack(&mut self) {
        self.flag_ack = true;
    }

    pub fn set_command(&mut self) {
        self.packet_type = RaniType::Command;
    }

    pub fn deserialise(buf: &[u8]) -> Result<(RaniHeader, &[u8]), &str> {
        if buf.len() < HEADER_SIZE {
            return Err("header buffer not large enough");
        }
        if Self::verify_checksum(buf).is_err() {
            return Err("invalid checksum");
        }
        let payload = &buf[HEADER_SIZE..];
        let buf = &buf[..HEADER_SIZE];

        let src = buf[0];
        let dest = buf[1];
        let payload_length = buf[2] - HEADER_SIZE as u8;

        let flag_err = (buf[3] & (1 << FLAG_BIT_ERR)) != 0;
        let flag_end = (buf[3] & (1 << FLAG_BIT_END)) != 0;
        let flag_ack = (buf[3] & (1 << FLAG_BIT_ACK)) != 0;
        let ttl = buf[3] & 0x0F;

        let packet_type: RaniType = ((buf[4] & 0xF0) >> 4).try_into()?;
        let seq_no: u16 = (((buf[4] & 0x0F) as u16) << 8) | buf[5] as u16;

        let mut hdr = RaniHeader::new(src, dest, payload_length, ttl, seq_no);
        hdr.flag_err = flag_err;
        hdr.flag_end = flag_end;
        hdr.flag_ack = flag_ack;
        hdr.packet_type = packet_type;

        Ok((hdr, payload))
    }

    pub fn serialise(&self, payload: &[u8], buf: &mut [u8]) -> Result<(), &str> {
        if buf.len() < HEADER_SIZE {
            return Err("header buffer not large enough");
        }
        let buf = &mut buf[..HEADER_SIZE];
        buf.fill(0);

        buf[0] = self.src;
        buf[1] = self.dest;
        buf[2] = self.length;
        buf[3] = self.serialise_flags() | (self.ttl & 0x0F);
        buf[4] = ((self.packet_type as u8) << 4) |
            (((self.seq_no & 0x0F00) >> 8) as u8);
        buf[5] = (self.seq_no & 0x00FF) as u8;
        buf[6] = Self::checksum(buf, payload);

        Ok(())
    }

    fn serialise_flags(&self) -> u8 {
        ((if self.flag_err { 1 } else { 0 }) << FLAG_BIT_ERR)
        |   ((if self.flag_end { 1 } else { 0 }) << FLAG_BIT_END)
        |   ((if self.flag_ack { 1 } else { 0 }) << FLAG_BIT_ACK)
    }

    fn checksum(hdr_buf: &[u8], payload: &[u8]) -> u8 {
        !hdr_buf[..6].iter()
            .chain(payload.iter())
            .fold(0, |sum: u8, &byte| sum.wrapping_add(byte))
    }

    fn verify_checksum(buf: &[u8]) -> Result<(), ()> {
        if (!buf.iter()
            .fold(0, |sum: u8, &byte| sum.wrapping_add(byte)))
            == 0 { Ok(()) } else { Err(()) }
    }
}

impl RaniPacket {
    pub fn new(header: RaniHeader, payload: RaniPayload) -> Self {
        Self { header, payload }
    }

    pub fn size(&self) -> usize {
        self.header.size() + self.payload.size()
    }

    pub fn deserialise(buf: &[u8]) -> Result<(Self, &[u8]), &str> {
        let (header, buf) =  RaniHeader::deserialise(buf)?;
        let (payload, buf) = RaniPayload::deserialise(&header, buf)?;
        Ok((Self::new(header, payload), buf))
    }

    pub fn serialise(&self, buf: &mut [u8]) -> Result<(), &str> {
        if buf.len() < self.size() {
            return Err("packet buffer not large enough");
        }
        
        let payload_buf = &mut buf[HEADER_SIZE..self.size()];
        self.payload.serialise(payload_buf)?;

        let mut header_buf = [0u8; HEADER_SIZE];
        self.header.serialise(payload_buf, &mut header_buf)?;
        buf[..HEADER_SIZE].copy_from_slice(&header_buf);

        Ok(())
    }
}

impl RaniPayload {
    pub fn size(&self) -> usize {
        match self {
            Self::Command(payload) => payload.size(),
            Self::Data(payload) => payload.len(),
        }
    }

    fn deserialise<'a>(header: &RaniHeader, buf: &'a[u8]) -> Result<(Self, &'a[u8]), &'a str> {
        let payload_length = header.length as usize - HEADER_SIZE;
        if buf.len() < payload_length {
            return Err("payload buffer not large enough");
        }
        
        match header.packet_type {
            RaniType::Command => {
                let (payload, buf) = RaniCommandPayload::deserialise(buf)?;
                Ok((Self::Command(payload), buf))
            },

            RaniType::Data => {
                let payload = buf[..payload_length].to_vec();
                let buf = &buf[payload_length..];
                Ok((Self::Data(payload), buf))
            },
        }
    }

    fn serialise(&self, buf: &mut [u8]) -> Result<(), &str> {
        if buf.len() < self.size() {
            return Err("payload buffer not large enough");
        }
        
        match self {
            Self::Command(payload) => payload.serialise(buf)?,
            Self::Data(payload) => buf.copy_from_slice(payload),
        };

        Ok(())
    }
}

impl RaniCommandPayload {
    pub fn new(entry_count: u8, timestamp: u32, entries: Vec<RaniCommandEntry>) -> Self {
        assert_eq!(entry_count as usize, entries.len());
        assert!(timestamp < (1 << 20));
        Self { entry_count, timestamp, entries }
    }

    pub fn size(&self) -> usize {
        COMMAND_HEADER_SIZE + COMMAND_ENTRY_SIZE * self.entry_count as usize
    }

    fn deserialise(buf: &[u8]) -> Result<(Self, &[u8]), &str> {
        if buf.len() < COMMAND_HEADER_SIZE {
            return Err("command payload buffer not large enough");
        }
        let entry_count = buf[0];
        if buf.len() < COMMAND_HEADER_SIZE + COMMAND_ENTRY_SIZE * entry_count as usize {
            return Err("command payload buffer not large enough");
        }

        let timestamp = ((buf[1] as u32 & 0x0F) << 16) |
            ((buf[2] as u32) << 8) |
            (buf[3] as u32);
        
        let mut buf = &buf[COMMAND_HEADER_SIZE..];
        let mut entries = Vec::with_capacity(entry_count as usize);

        for _ in 0..entry_count {
            let (entry, next_buf) = RaniCommandEntry::deserialise(buf)?;
            entries.push(entry);
            buf = next_buf;
        }

        Ok((Self::new(entry_count, timestamp, entries), buf))
    }

    fn serialise(&self, buf: &mut [u8]) -> Result<(), &str> {
        if buf.len() < self.size() {
            return Err("command payload buffer not large enough");
        }

        buf[0] = self.entry_count;
        buf[1] = ((self.timestamp & 0x000F0000) >> 16) as u8;
        buf[2] = ((self.timestamp & 0x0000FF00) >> 8) as u8;
        buf[3] = (self.timestamp & 0x000000FF) as u8;

        for (i, entry) in self.entries.iter().enumerate() {
            entry.serialise(&mut buf[COMMAND_HEADER_SIZE + COMMAND_ENTRY_SIZE * i..])?;
        }

        Ok(())
    }
}

impl RaniCommandEntry {
    pub fn new(dest: u8, cost: u8) -> Self {
        Self { dest, cost }
    }

    fn deserialise(buf: &[u8]) -> Result<(Self, &[u8]), &str> {
        if buf.len() < COMMAND_ENTRY_SIZE {
            return Err("command entry buffer not large enough");
        }
        Ok((Self::new(buf[0], buf[1]), &buf[COMMAND_ENTRY_SIZE..]))
    }

    fn serialise(&self, buf: &mut [u8]) -> Result<(), &str> {
        if buf.len() < COMMAND_ENTRY_SIZE {
            return Err("command entry buffer not large enough");
        }
        buf[0] = self.dest;
        buf[1] = self.cost;
        Ok(())
    }
}

/**************************
 * FUNCTIONS
***************************/

pub fn validate_packets(a: &RaniPacket, b: &RaniPacket) -> Result<(), String> {
    macro_rules! return_if_neq {
        ($a:expr, $b:expr, $field:expr) => {
            if $a != $b { return Err(format!("{} not correct", $field)); }
        };
    }

    return_if_neq!(a, b, "packet");

    Ok(())
}
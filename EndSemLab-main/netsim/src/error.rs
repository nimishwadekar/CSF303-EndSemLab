use std::{
    net::{TcpListener, TcpStream, IpAddr},
    io::Write,
    sync::{Arc, Mutex}, collections::HashMap,
};

/**************************
 * MACROS
***************************/

#[macro_export]
macro_rules! crash {
    ($($arg:tt)*) => {{
        eprintln!("[*] <{}:{}> FATAL ERROR {}", file!(), line!(), format_args!($($arg)*));
        std::process::exit(1);
    }};
}

#[macro_export]
macro_rules! expect_result_or_crash {
    ($wrapped:expr, $($arg:tt)*) => {{
        match $wrapped {
            Ok(v) => v,
            Err(e) => crash!("{} : {}", format_args!($($arg)*), e),
        }
    }};
}

#[macro_export]
macro_rules! expect_option_or_crash {
    ($wrapped:expr, $($arg:tt)*) => {{
        match $wrapped {
            Some(v) => v,
            None => crash!($($arg)*),
        }
    }};
}

#[macro_export]
macro_rules! err_return {
    ($e:expr, $($args:tt)*) => {{
        match $e {
            Ok(v) => v,
            Err(e) => {
                eprintln!("[!] {}: {}", format_args!($($args)*), e);
                return Err(());
            },
        }
    }};
}

/**************************
 * STRUCTURES
***************************/

pub struct ErrorHandler {
    streams: HashMap<IpAddr, TcpStream>,
}

/**************************
 * IMPLEMENTATIONS
***************************/

impl ErrorHandler {
    pub fn new() -> Self {
        Self {
            streams: HashMap::new(),
        }
    }

    pub fn send_message(&mut self, ip: IpAddr, msg: String) -> std::io::Result<()> {
        let stream = expect_option_or_crash!(self.streams.get_mut(&ip), "error stream does not exist");

        stream.write_all(msg.as_bytes())?;
        stream.flush()
    }
}

/**************************
 * FUNCTIONS
***************************/

pub fn error_listen_thread(listener: TcpListener, error_handler: Arc<Mutex<ErrorHandler>>) {
    loop {
        let (stream, addr) = match listener.accept() {
            Ok(result) => result,
            Err(e) => {
                eprintln!("[*] WARN: error accept failed: {}", e);
                continue;
            },
        };

        expect_result_or_crash!(error_handler.lock(), "error_handler mutex lock")
            .streams.insert(addr.ip(), stream);
        eprintln!("[*] Opened error message stream with {}", addr.ip());
    }
}
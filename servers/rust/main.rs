#![feature(tcp)]

use std::net::{TcpListener, TcpStream};
use std::thread;
use std::io;
use std::env;
use std::mem;
use std::str::FromStr;
use std::io::Read;
use std::io::Write;

fn main() {
    let port_ : Option<String> = env::args().skip(2).next();
    if let None = port_ {
        println!("usage: {} -p port", env::args().next().unwrap());
        return;
    }

    let port = u16::from_str(&port_.unwrap());
    if let Err(ref e) = port {
        println!("invalid port number given: {}", e);
        return;
    }

    let listener = TcpListener::bind(("127.0.0.1", port.unwrap()));
    if let Err(ref e) = listener {
        println!("failed to listen on port: {}", e);
        return;
    }

    for stream in listener.unwrap().incoming() {
        match stream {
            Ok(stream) => {
                thread::spawn(move|| {
                    handle_client(stream)
                });
            }
            Err(e) => {
                println!("failed to accept connection: {}", e);
            }
        }
    }
}

fn handle_client(mut stream: TcpStream) {
    let mut buf = [0u8; 4];
    let mut challenge = 0u32;
    let mut nread;

    let _ = stream.set_nodelay(true);

    loop {
        nread = 0;
        while nread < buf.len() {
            match stream.read(&mut buf[nread..]) {
                Ok(n) if n == 0 => return,
                Ok(n) => nread += n,
                Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
                Err(e) => {
                    println!("{}", e);
                    return;
                }
            }
        }

        challenge = unsafe { mem::transmute(buf) };
        challenge = u32::from_be(challenge);
        if challenge == 0 {
            std::process::exit(0);
        }
        challenge = u32::to_be(challenge + 1);
        buf = unsafe { mem::transmute(challenge) };

        let mut nwritten = 0;
        while nwritten < buf.len() {
            match stream.write(&buf[nwritten..]) {
                Ok(n) => nwritten += n,
                Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
                Err(e) => {
                    println!("{}", e);
                    return;
                }
            }
        }
    }
}

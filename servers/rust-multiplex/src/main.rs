#![feature(tcp)]

extern crate num_cpus;

use std::sync::mpsc::{Sender, TryRecvError};
use std::sync::mpsc;
use std::net::{TcpListener, TcpStream};
use std::thread;
use std::io;
use std::process::Command;
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

    let listener_ = TcpListener::bind(("127.0.0.1", port.unwrap()));
    if let Err(ref e) = listener_ {
        println!("failed to listen on port: {}", e);
        return;
    }
    let listener = listener_.unwrap();

    // ideally, we'd use the num_cpus crate, but we can't until
    // https://github.com/seanmonstar/num_cpus/issues/12
    // is fixed.
    // let ncores = num_cpus::get() as u16;
    let ncores = String::from_utf8(Command::new("nproc").output().unwrap_or_else(|e| {
        panic!("failed to get number of cores: {}", e)
    }).stdout).unwrap_or_else(|e| {
        panic!("failed to get number of cores: {}", e)
    }).trim().parse::<usize>().unwrap_or_else(|e| {
        panic!("failed to get number of cores: {}", e)
    });
    let mut stream_txs: Vec<Sender<TcpStream>> = Vec::with_capacity(ncores);

    // spawn n theads, each multiplexing between many clients
    for _ in 0..ncores {
        let (stream_tx, stream_rx) = mpsc::channel();
        stream_txs.push(stream_tx);

        thread::spawn(move || {
            let mut streams : Vec<TcpStream> = Vec::new();
            let mut done : Vec<usize> = Vec::new();

            loop {
                let stream_ = stream_rx.try_recv();
                match stream_ {
                    Ok(stream) => streams.push(stream),
                    Err(TryRecvError::Empty) => (),
                    Err(TryRecvError::Disconnected) => () /* TODO */,
                }

                done.clear();
                for (i, mut stream) in streams.iter_mut().enumerate() {
                    if !iterate(stream) {
                        done.push(i);
                    }
                }

                // need to remove in reverse order to ensure the indices remain correct
                done.reverse();
                for i in done.iter() {
                    streams.remove(*i);
                }
            }
        });
    }

    // accept on all cores
    let mut wait = Vec::new();
    for i in 0..ncores  {
        let listener_clone = listener.try_clone().unwrap();
        let txs = stream_txs.clone();

        wait.push(thread::spawn(move || {
            let mut ti = i as usize;
            loop {
                let stream_ = listener_clone.accept();

                match stream_ {
                    Ok((mut stream,_)) => {
                        prepare_connection(&mut stream);
                        if let Err(ref e) = txs[ti as usize].send(stream) {
                            println!("failed to delegate stream: {}", e);
                        }

                    }
                    Err(e) => {
                        println!("failed to accept connection: {}", e);
                    }
                }
                
                ti = (ti+1) % txs.len();
            }
        }));
    }

    for t in wait {
        let _ = t.join();
    }
}

#[inline(always)]
fn iterate(stream: &mut TcpStream) -> bool {
    let mut buf = [0u8; 4];
    let mut challenge;
    let mut nread;

    nread = 0;
    while nread < buf.len() {
        match stream.read(&mut buf[nread..]) {
            Ok(n) if n == 0 => return false,
            Ok(n) => nread += n,
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => {
                println!("{}", e);
                return false;
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
                return false;
            }
        }
    }

    return true;
}

fn prepare_connection(stream: &mut TcpStream) -> bool {
    let _ = stream.set_nodelay(true);
    iterate(stream)
}

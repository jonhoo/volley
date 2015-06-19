#![feature(tcp)]

extern crate num_cpus;

use std::sync::mpsc::{Sender, Receiver};
use std::sync::mpsc;
use std::net::{TcpListener, TcpStream};
use std::thread;
use std::io;
use std::env;
use std::mem;
use std::str::FromStr;
use std::io::Read;
use std::io::Write;


fn main() {

    //Get the port.
    let port_str_ : Option<String> = env::args().skip(2).next();
    if let None = port_str_ {
        println!("usage: {} -p port -c cores", env::args().next().unwrap());
        return;
    }

    let port_ = u16::from_str(&port_str_.unwrap());
    if let Err(ref e) = port_ {
        println!("invalid port number given: {}", e);
        return;
    }

    let port = port_.unwrap();

    let ncores = num_cpus::get() as u16;
    println!("Number of cores: {}", ncores);


    let mut work_tx_vec: Vec<Sender<TcpStream>> = Vec::with_capacity(ncores as usize);

    for _ in 0..ncores {
        let (work_tx, work_rx): (Sender<TcpStream>, Receiver<TcpStream>) = mpsc::channel();
        work_tx_vec.push(work_tx);

        {

            thread::spawn(move || {

                let mut streams:Vec<TcpStream>=Vec::new();

                loop {

                    let stream_ = work_rx.try_recv();
                    match stream_ {
                        Ok(stream) => streams.push(stream),
                        Err(_) => ()
                    }

                    let mut to_be_removed:Vec<u32>=Vec::new();
                    let mut index:u32=0;
                    for mut stream in & mut streams {
                        if handle_client(& mut stream) {
                            to_be_removed.push(index);
                        }
                        index+=1;
                    }
                    for i in to_be_removed {
                        streams.remove(i as usize);
                    }
                }
            });
        }
    }
    println!("{} threads created.", ncores);


    let listener_ = TcpListener::bind(("127.0.0.1", port));
    if let Err(ref e) = listener_ {
        println!("failed to listen on port: {}", e);
        return;
    }
    let listener = listener_.unwrap();

    println!("Started listening on port: {}",port);

    let (init_tx_, init_rx): (Sender<TcpStream>, Receiver<TcpStream>) = mpsc::channel();

    //Perform the initialization phase.
    for _ in 0..ncores  {
        let init_tx = init_tx_.clone();
        let listener_clone = listener.try_clone().unwrap();

        thread::spawn(move || {
            loop {
                let stream_ = listener_clone.accept();
                match stream_ {
                    Ok((mut stream,_)) => {
                        initialize_connection(&mut stream);
                        let stream_sent = init_tx.send(stream);
                        if let Err(ref e) = stream_sent {
                            println!("Channel Error: {}", e);
                        }

                    }
                    Err(e) => {
                        println!("failed to accept connection: {}", e);
                    }
                }
            }
        });

    }

    //Loop in case we need more statistical data.
    loop {

        //Perform the network processing by the ncores threads.

        for index in 0..ncores {
            let stream = init_rx.recv();
            if let Err(ref e) = stream {
                println!("Channel Error: {}", e);
            }

            let stream_sent = work_tx_vec[index as usize].send(stream.unwrap());
            if let Err(ref e) = stream_sent {
                println!("Channel Error: {}", e);
            }
        }
    }
}

fn handle_client(stream: & mut TcpStream) -> bool {
    let mut buf = [0u8; 4];
    let mut challenge;
    let mut nread;

    nread = 0;
    while nread < buf.len() {
        match stream.read(&mut buf[nread..]) {
            Ok(n) if n == 0 => return true,
            Ok(n) => {nread += n;
            },
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => {
                println!("{}", e);
                return true;
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
            Ok(n) => {nwritten += n;
            },
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => {
                println!("{}", e);
                return true;
            }
        }
    }
    return false;
}



fn initialize_connection(stream: & mut TcpStream) {
    let mut buf = [0u8; 4];
    let mut challenge;
    let mut nread;

    let _ = stream.set_nodelay(true);


    nread = 0;
    while nread < buf.len() {
        match stream.read(&mut buf[nread..]) {
            Ok(n) if n == 0 => return,
            Ok(n) => {nread += n;
            },
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
            Ok(n) => {nwritten += n;
            },
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => {
                println!("{}", e);
                return;
            }
        }
    }
}

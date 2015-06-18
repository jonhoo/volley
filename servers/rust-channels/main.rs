#![feature(tcp)]

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

    let ncores_str_ :Option<String> = env::args().skip(4).next();

    if let None = ncores_str_ {
        println!("usage: {} -p port -c cores", env::args().next().unwrap());
        return;
    }

    let ncores_ = u16::from_str(&ncores_str_.unwrap());
    if let Err(ref e) = ncores_ {
        println!("invalid cores number given: {}", e);
        return;
    }

    let ncores = ncores_.unwrap();
    println!("Number of cores: {}", ncores);

    let nclients_str_ :Option<String> = env::args().skip(6).next();

    if let None = nclients_str_ {
        println!("usage: {} -p port -c cores", env::args().next().unwrap());
        return;
    }

    let nclients_ = u16::from_str(&nclients_str_.unwrap());
    if let Err(ref e) = nclients_ {
        println!("invalid cores number given: {}", e);
        return;
    }

    let nclients = nclients_.unwrap();
    println!("Number of clients: {}", nclients);


    let (coord_tx_, coord_rx): (Sender<u16>, Receiver<u16>) = mpsc::channel();

    let mut work_tx_vec: Vec<Sender<TcpStream>> = Vec::with_capacity(ncores as usize);

    for id in 0..ncores {
        let (work_tx, work_rx_): (Sender<TcpStream>, Receiver<TcpStream>) = mpsc::channel();
        work_tx_vec.push(work_tx);

        {
            let work_rx = work_rx_; 
            let coord_tx = coord_tx_.clone();

            thread::spawn(move || {

                loop {
                    let ready= coord_tx.send(id);
                    if let Err(ref e) = ready {
                        println!("Channel Error: {}", e);
                    }

                    let stream = work_rx.recv();
                    if let Err(ref e) = stream {
                        println!("Channel Error: {}", e);
                    }
                    //        println!("handle client started");
                    handle_client(stream.unwrap());
                    //        println!("handle client finished");
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

    loop {

        let mut streams: Vec<TcpStream> = Vec::new();
        for _ in 0..nclients  {
            let stream_ = listener.accept();
            match stream_ {
                Ok((stream,_)) => {
                    //           println!("Received a connection");
                    let stream_clone = stream.try_clone().unwrap();
                    thread::spawn(move || {
                        initialize_connection(stream_clone);
                    });
                    streams.push(stream);
                }
                Err(e) => {
                    println!("failed to accept connection: {}", e);
                }
            }

        }

        for stream in streams {
            let ready = coord_rx.recv();
            if let Err(ref e) = ready {
                println!("Channel Error: {}", e);
            }

            let id = ready.unwrap();
            let stream_sent = work_tx_vec[id as usize].send(stream);
            if let Err(ref e) = stream_sent {
                println!("Channel Error: {}", e);
            }

        }
    }
}

fn handle_client(mut stream: TcpStream) {
    let mut buf = [0u8; 4];
    let mut challenge;
    let mut nread;

    let _ = stream.set_nodelay(true);

    loop  {

        nread = 0;
        while nread < buf.len() {
            match stream.read(&mut buf[nread..]) {
                Ok(n) if n == 0 => return,
                Ok(n) => {nread += n;
                    //            println!("n:{},nread:{}",n,nread)
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
                    //          println!("nwritten:{}",nwritten);
                },
                Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
                Err(e) => {
                    println!("{}", e);
                    return;
                }
            }
        }
    }
}



fn initialize_connection(mut stream: TcpStream) {
    let mut buf = [0u8; 4];
    let mut challenge;
    let mut nread;

    let _ = stream.set_nodelay(true);


    nread = 0;
    while nread < buf.len() {
        match stream.read(&mut buf[nread..]) {
            Ok(n) if n == 0 => return,
            Ok(n) => {nread += n;
                //            println!("n:{},nread:{}",n,nread)
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
                //          println!("nwritten:{}",nwritten);
            },
            Err(ref e) if e.kind() == io::ErrorKind::Interrupted => {}
            Err(e) => {
                println!("{}", e);
                return;
            }
        }
    }
    //       println!("fininshed initialization");
}

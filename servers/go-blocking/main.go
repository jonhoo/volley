package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"log"
	"net"
	"os"
	"os/signal"
	"runtime"
	"syscall"
)

func main() {
	var port = flag.Int("p", 0, "port to listen on")
	var connCount = flag.Int("c", 0, "number of connections")
	flag.Parse()

	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		for range c {
			os.Exit(0)
		}
	}()

	runtime.GOMAXPROCS(*connCount)
	ln, err := net.Listen("tcp", fmt.Sprintf("localhost:%d", *port))
	if err != nil {
		log.Printf("failed to listen on port %d: %v", port, err)
		return
	}

	for {
		conn, err := ln.Accept()
		if err != nil {
			log.Println("failed to accept connection:", err)
			continue
		}
		go handleConnection(conn)
	}
}

func handleConnection(c net.Conn) {
	runtime.LockOSThread()
	f, err := c.(*net.TCPConn).File()
	c.Close()
	if err != nil {
		log.Println(err)
	}
	defer f.Close()

	var (
		challenge uint32
		buf       = make([]byte, 4)
	)

	for {
		if _, err = f.Read(buf); err != nil {
			log.Println("read error: ", err)
			return
		}

		if challenge = binary.BigEndian.Uint32(buf); challenge == 0 {
			os.Exit(0)
		}
		binary.BigEndian.PutUint32(buf, challenge+1)

		if _, err = f.Write(buf); err != nil {
			log.Println("write error: ", err)
			return
		}
	}
}

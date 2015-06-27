package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"log"
	"net"
	"os"
	"os/signal"
	"runtime"
	"sync/atomic"
	"syscall"
)

var (
	numConns   int32 = 0
	numThreads int
)

func main() {
	var port = flag.Int("p", 0, "port to listen on")
	flag.Parse()

	c := make(chan os.Signal, 1)
	signal.Notify(c, syscall.SIGINT, syscall.SIGTERM)
	go func() {
		for range c {
			os.Exit(0)
		}
	}()

	numThreads = runtime.NumCPU()
	runtime.GOMAXPROCS(numThreads)

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
		nc := atomic.AddInt32(&numConns, 1)
		if int(nc) >= numThreads {
			numThreads *= 2
			runtime.GOMAXPROCS(numThreads)
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
		return
	}

	defer func() {
		f.Close()
		atomic.AddInt32(&numConns, -1)
	}()

	var (
		challenge uint32
		buf       = make([]byte, 4)
	)

	for {
		if _, err = f.Read(buf); err != nil {
			if err != io.EOF {
				fmt.Println("read error: ", err)
			}
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

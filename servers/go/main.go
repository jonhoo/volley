package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"net"
	"os"
	"os/signal"
	"runtime"
	"syscall"
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

	runtime.GOMAXPROCS(runtime.NumCPU())
	ln, err := net.Listen("tcp", fmt.Sprintf("localhost:%d", *port))
	if err != nil {
		fmt.Printf("failed to listen on port %d: %v\n", port, err)
		return
	}

	for {
		conn, err := ln.Accept()
		if err != nil {
			fmt.Println("failed to accept connection:", err)
			continue
		}
		go handleConnection(conn)
	}
}

func handleConnection(c net.Conn) {
	defer c.Close()

	var challenge uint32 = 0
	for {
		if err := binary.Read(c, binary.BigEndian, &challenge); err != nil {
			if err != io.EOF {
				fmt.Println("bad read", err)
			}
			return
		}
		challenge++
		if err := binary.Write(c, binary.BigEndian, &challenge); err != nil {
			fmt.Println("bad write", err)
			return
		}
	}
}

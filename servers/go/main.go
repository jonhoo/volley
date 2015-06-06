package main

import (
	"encoding/binary"
	"flag"
	"fmt"
	"io"
	"net"
)

func main() {
	var port = flag.Int("p", 0, "port to listen on")
	flag.Parse()

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

package main

import (
	"encoding/binary"
	"flag"
	"log"
	"os"
	"os/signal"
	"runtime"
	"syscall"

	"golang.org/x/sys/unix"
)

func listen(port int) int {
	fd, err := unix.Socket(unix.AF_INET, unix.SOCK_STREAM, unix.IPPROTO_TCP)
	if err != nil {
		panic(err)
	}
	err = unix.SetsockoptInt(fd, unix.SOL_SOCKET, unix.SO_REUSEADDR, 1)
	addr := unix.SockaddrInet4{Port: port, Addr: [4]byte{0, 0, 0, 0}}
	err = unix.Bind(fd, &addr)
	if err != nil {
		panic(err)
	}
	err = unix.Listen(fd, 4096)
	if err != nil {
		panic(err)
	}
	return fd
}

func handler(ch chan int) {
	var fds []int
	var done []int
	buf := make([]byte, 4)

	for {
		if len(fds) == 0 {
			fds = append(fds, <-ch)
		} else {
			select {
			case fd := <-ch:
				fds = append(fds, fd)
			default:
			}
		}

		for i, fd := range fds {
			if !iterate(fd, buf) {
				done = append(done, i)
			}
		}

		for j := len(done) - 1; j >= 0; j-- {
			p := done[j]
			copy(fds[p:], fds[p+1:])
			fds = fds[:len(fds)-1]
		}
		done = done[:0]
	}
}

func iterate(fd int, buf []byte) bool {
	nread, err := unix.Read(fd, buf)
	if err != nil {
		log.Println(err)
		return false
	}
	if nread == 0 { // EOF
		return false
	}
	for nread < 4 {
		n, err := unix.Read(fd, buf[nread:])
		if err != nil {
			log.Println(err)
			return false
		}
		nread += n
	}

	challenge := binary.BigEndian.Uint32(buf)
	if challenge == 0 {
		os.Exit(0)
	}
	binary.BigEndian.PutUint32(buf, challenge+1)

	nwritten := 0
	for nwritten < 4 {
		n, err := unix.Write(fd, buf[nwritten:])
		if err != nil {
			log.Println(err)
			return false
		}
		nwritten += n
	}
	return true
}

func serve(fd int, ch []chan int) {
	buf := make([]byte, 4)
	l := len(ch)
	for i := 0; ; i++ {
		nfd, _, err := unix.Accept(fd)
		if err != nil {
			log.Println(err)
			continue
		}
		err = unix.SetsockoptInt(nfd, unix.SOL_TCP, unix.TCP_NODELAY, 1)
		if err != nil {
			log.Println(err)
		}
		if iterate(nfd, buf) {
			ch[i%l] <- nfd
		} else {
			unix.Close(nfd)
		}
	}
}

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

	NP := runtime.NumCPU()
	runtime.GOMAXPROCS(NP + 1)

	sfd := listen(*port)

	chs := make([]chan int, NP)
	for i := 0; i < NP; i++ {
		chs[i] = make(chan int, 4)
		go handler(chs[i])
	}
	serve(sfd, chs)
}

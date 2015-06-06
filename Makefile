target/client: client/main.c
	mkdir -p target
	clang -Wall -g -o target/client -pthreads -lm -lbsd -O3 client/main.c

clean:
	rm -rf target

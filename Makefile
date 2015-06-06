target/client: client/main.c
	mkdir -p target
	clang -o target/client -pthreads -lm -lbsd client/main.c

clean:
	rm -rf target

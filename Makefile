all: target/client $(addprefix target/,$(wildcard servers/*))

target/servers/%: servers/$*/**/*.*
	mkdir -p target/servers
	make -C servers/$* $*
	cp servers/$*/$* $@

target/client: client/main.c
	mkdir -p target
	clang -Wall -g -o target/client -pthreads -lm -lbsd -O3 client/main.c

clean:
	rm -rf target
	$(foreach dir,$(wildcard servers/*),make -C $(dir) clean;)

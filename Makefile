all: target/client $(addprefix target/,$(wildcard servers/*))

target/servers/%: servers/$*/**/*.*
	mkdir -p target/servers
	make -C servers/$* $*
	cp servers/$*/$* $@

target/client: client/main.c
	mkdir -p target
	$(CC) -Wall -g -o target/client -pthread -lm -lbsd -O3 -std=gnu11 client/main.c

clean:
	rm -rf target
	$(foreach dir,$(wildcard servers/*),make -C $(dir) clean;)

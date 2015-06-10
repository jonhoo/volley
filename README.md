# Volley

Volley is a benchmarking tool for measuring the performance (latency in
particular) of server networking stacks. It can be used to determine the
performance of the lower levels of the stack (i.e. the kernel), of the
server application's programming language abstractions for sockets, or
of the libraries and code running within the application.

Volley spawns a configurable number of concurrent clients that all
connect to the server, and then performs ping-pong-like single-word
operations with the server to gather statistics. It will continue doing
so for as many iterations as it takes to produce statistically relevant
results.

## Preliminary results

Running the volley benchmarks on an 80-core machine running Linux 3.16
with 40 and 80 clients distributed across 40 cores yields the results
given in the graph below. Error bars denote the 95% confidence interval.

![performance plot](https://cdn.rawgit.com/jonhoo/volley/249f1d55a5a12f925d560bda069f9ce8b56c1dd1/benchmark/perf.png)

To reproduce, run:

```
benchmark/ $ experiment -r $(PWD)/..
benchmark/ $ grep us 'out/*/run-1/stdout.log' | tr '/:-' '\t' | awk '{print $2" "$3" "$4" "$8}' > plot.dat
```

And plot using the R script in `benchmark/plot.R`. Experiment can be
found [here](https://github.com/jonhoo/experiment).


## Contributing servers

Please submit PRs adding a directory to `servers/`. The name of the
directory should be indicative of what server is being tested. The
directory should contain a Makefile that has (at least) two targets:

  - a target with the same name as the directory. this rule should
    produce an executable binary with that name in the current
    directory.
  - a `clean` rule, which is called whenever the top-level `clean`
    target is invoked.

The binary accepts a single, mandatory flag, `-p`, which names a TCP
port. The server should listen on this port for incoming requests. For
every established connection, the server behaviour should be as follows:

  1. read 4 bytes from the connection
  2. parse the 4 bytes into a 32 bit integer using network byte order
  3. if the integer is zero, the server should terminate
  4. add one to the integer, wrapping around if necessary
  5. write the integer as 4 bytes in network byte order to the connection
  6. go to step 1.

If a connection is closed, the server should continue waiting for new
connections. If the server receives a SIGTERM, it should terminate at
its earliest convenience (i.e. it should not block indefinitely waiting
for new connctions).

To verify that your server works correctly, run the follwoing commands:

```
volley/ $ make
volley/ $ ./benchmark/bench.sh 1 1 1 target/servers/<your-server>
```

This should output something akin to:

```
numactl -C +0-0 /home/.../volley/target/servers/<server> -p 2222
numactl -C 3-3 /home/.../volley/target/<server> -p 2222 -c 1
priming with 1000000 iterations across 1 clients
iteration complete: mean is 9us, stddev is 1.25us
9.04us
```

You may also see the error:

```
./benchmark/bench.sh: line 47: kill: (12345) - No such process
```

This can safely be ignored, and is an artefact of the fact that the
server may terminate when it receives a 0 challenge.

## Server improvements

If you believe an already implemented server could be improved (either
syntactically or semantically), please file an issue detailing the
problem, and if possible, submit a PR giving a proposed solution.

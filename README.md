# Volley

Volley is a benchmarking tool for measuring the performance (latency in
particular) of server networking stacks. It can be used to determine the
performance of the lower levels of the stack (i.e. the kernel), of the
server application's programming language abstractions for sockets, or
of the libraries and code running within the application.

Volley spawns a configurable number of concurrent clients that all
connect to the server, and then performs ping-pong-like single-byte
operations with the server to gather statistics. It will continue doing
so for as many iterations as it takes to produce statistically relevant
results.

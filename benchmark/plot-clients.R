#!/usr/bin/env Rint
library(grDevices)
library(utils)
X11(width=12, height=10)

library(ggplot2)
args <- commandArgs(trailingOnly = TRUE)
args <- if (length(args) == 0) Sys.getenv("ARGS") else args
args <- if (args[1] == "") "plot-clients.dat" else args

d <- data.frame(read.table(
			   text=gsub('us ', ' ', readLines(file(args[1]))),
			   col.names=c("server", "clients", "cores", "time", "stddev", "n")
			   ))

d$ci = 2.58 * d$stddev / sqrt(d$n)
d$ops = d$clients/(d$time/1000.0/1000.0)
d$min = d$clients/((d$time-d$ci)/1000.0/1000.0)
d$max = d$clients/((d$time+d$ci)/1000.0/1000.0)

#d = d[d[, "clients"] == 80,]
#d = d[grep("^rust", d[, "server"]),]
print(d)
p <- ggplot(data=d, aes(x = clients, y = ops, ymin = min, ymax = max, color = server), log="x")
p <- p + geom_line()
p <- p + ylim(0, 3000000)
p <- p + geom_errorbar()
p <- p + facet_wrap(~ cores)
p <- p + xlab("Clients")
p <- p + ylab("Mean ops/s")

p
ggsave("plot-clients.png", plot = p, width = 8, height = 6)

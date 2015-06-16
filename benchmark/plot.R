#!/usr/bin/env Rint
library(grDevices)
library(utils)
X11(width=12, height=10)

library(ggplot2)
args <- commandArgs(trailingOnly = TRUE)
args <- if (length(args) == 0) Sys.getenv("ARGS") else args
args <- if (args[1] == "") "plot.dat" else args

d <- data.frame(read.table(
			   text=gsub('us$', '', readLines(file(args[1]))),
			   col.names=c("server", "clients", "cores", "time")
			   ))
d$ops = d$clients/(d$time/1000.0/1000.0)
d$min = d$clients/((d$time-5)/1000.0/1000.0)
d$max = d$clients/((d$time+5)/1000.0/1000.0)

#d = d[d[, "clients"] == 80,]
#d = d[grep("^go", d[, "server"]),]
print(d)
p <- ggplot(data=d, aes(x = cores, y = ops, ymin = min, ymax = max, color = server))
p <- p + geom_line()
p <- p + ylim(0, 1600000)
p <- p + geom_errorbar()
p <- p + facet_wrap(~ clients)
p <- p + xlab("CPU cores")
p <- p + ylab("Mean ops/s")

p
ggsave("perf.png", plot = p, width = 8, height = 6)

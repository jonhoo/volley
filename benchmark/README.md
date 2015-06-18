Files in this directory:

  - bench.sh: script for running a single performance benchmark.
    Invoked as:

     ```
     $ ./bench.sh server_cores client_cores clients program args...
     ```

  - experiment.json: file describing which benchmarks to run. Uses
    [experiment](https://github.com/jonhoo/experiment) to run the
    benchmarks automatically.
  - perf.png: Most recent benchmarking results
  - plot.dat: Datafile containing raw data used to plot `perf.png`.
    Columns are:

     ```
     server clients cores mean stddev num_samples
     ```

  - plot.R: [R](http://www.r-project.org/) program for creating
    `perf.png` from `plot.dat`. `Rint` is simply:

     ```
     f=$1; shift; env "R_PROFILE_USER=$f" "ARGS=$@" R --no-save -q
     ```

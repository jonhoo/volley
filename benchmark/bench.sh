#!/bin/bash
command -v numactl >/dev/null 2>&1
no_numa=$?

if [ $no_numa -eq 1 ]; then
	echo "no numactl, so cannot force core locality; exiting..." > /dev/stderr
	exit 1
fi

scores="$1"; shift
ccores="$1"; shift
clients="$1"; shift
((needed=scores+ccores));

# where are we?
DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

cores=$(numactl -H | grep cpus | sed 's/.*: //' | paste -sd' ')
cores=($cores)
ncores=${#cores[@]}

if ((needed>ncores)); then
	echo "Cannot use more server+client cores ($needed) than there are CPU cores ($ncores)" >/dev/stderr
	exit 1
fi

srange=$(echo ${cores[@]} | cut -d' ' -f1-${scores} | tr ' ' ',')
crange=$(echo ${cores[@]} | rev | cut -d' ' -f1-${ccores} | rev | tr ' ' ',')

args=()
for i in "$@"; do
	if [ "$i" == "CCORES" ]; then
		args+=("$ccores")
	elif [ "$i" == "SCORES" ]; then
		args+=("$scores")
	else
		args+=("$i")
	fi
done

# run the server
echo numactl -C $srange "${args[@]}" -p 2222 >/dev/stderr
numactl -C $srange "${args[@]}" -p 2222 &
pid=$!

# let it initialize
sleep 1

# find the client binary
client=$(readlink -f "$DIR/../target/client")

# run the client
echo numactl -C $crange "$client" -p 2222 -c $clients >/dev/stderr
numactl -C $crange "$client" -p 2222 -c $clients

# terminate the server
kill $pid 2>/dev/null
wait $pid 2>/dev/null

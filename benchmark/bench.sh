#!/bin/bash
DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
client=$(readlink -f "$DIR/../target/client")

ncores=$(lscpu | grep "^CPU(s):" | sed 's/.* //')
scores="$1"; shift
ccores="$1"; shift
clients="$1"; shift
((startc=ncores-ccores))

if ((scores+ccores>ncores)); then
	echo "Cannot use more server+client cores than there are CPU cores" >/dev/stderr
	exit 1
fi

((ends=scores-1))
((endc=ncores-1))

command -v numactl >/dev/null 2>&1
no_numa=$?

if [ $no_numa -eq 1 ]; then
	echo "no numactl, so cannot force core locality; exiting..." > /dev/stderr
	exit 1
fi

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

echo numactl -C 0-$ends "${args[@]}" -p 2222 >/dev/stderr
numactl -C 0-$ends "${args[@]}" -p 2222 &
pid=$!

sleep 1

echo numactl -C $startc-$endc "$client" -p 2222 -c $clients >/dev/stderr
numactl -C $startc-$endc "$client" -p 2222 -c $clients

kill $pid
wait $pid 2>/dev/null

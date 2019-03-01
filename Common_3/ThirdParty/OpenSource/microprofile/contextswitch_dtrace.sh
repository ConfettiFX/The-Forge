#!/bin/sh
# DTrace is gated by system integrity protection starting from OSX 10.11 - it needs to be disabled for this script to work

PIPE=/tmp/microprofile-contextswitch

if [ "$EUID" -ne 0 ];
then
	sudo $0
	exit
fi

while true;
do
	echo "DTrace run; output file: $PIPE"
	mkfifo $PIPE
	dtrace -q -n fbt::thread_dispatch:return'{printf("MPTD %x %x %x %x\n", cpu, pid, tid, machtimestamp)}' -o $PIPE
	rm $PIPE
done

#!/bin/bash

FILE=/mnt/data0/sasha/testfile

echo $FILE

for TEST in readsyscall readmmap
do
    echo ${TEST}
    for BLOCK in 512 1024 2048 4096 8192 16384
    do
	for i in {1..3}
	do
	    sudo sync; echo 1 > /proc/sys/vm/drop_caches
	    ./fa -b ${BLOCK} --${TEST} -f ${FILE} --silent
	done
    done
done


#!/bin/bash

#FILE=/mnt/data0/sasha/testfile
FILE=/mnt/pmem/testfile

echo $FILE

PERF="perf record -g -e page-faults -e dTLB-load-misses -e LLC-load-misses  -e offcore_requests.all_data_rd -e offcore_response.all_code_rd.llc_miss.any_response -e kmem:* -e filemap:* -e huge_memory:* -e pagemap:* -e dtlb_load_misses.walk_completed_1g -e dtlb_load_misses.walk_completed_2m_4m -e dtlb_load_misses.walk_completed_4k -e dtlb_load_misses.walk_completed  -e dtlb_load_misses.walk_duration -e dtlb_load_misses.miss_causes_a_walk -e dtlb_load_misses.stlb_hit -e dtlb_load_misses.stlb_hit_2m -e dtlb_load_misses.stlb_hit_4k -e page-faults -e major-faults -e minor-faults -e cycles -e ext4:* -e vmscan:*"
PERF="perf record -e cycles -e dTLB-loads -e dTLB-load-misses -e page-faults"

drop_caches() {
    (echo 1) > /proc/sys/vm/drop_caches;
}

# Use the block below to run a single test while collecting profiling info
PROFILING_RUN=0
if [ ${PROFILING_RUN} = 1 ]
then
   echo "Doing a profiling run..."
   BLOCK=16384
   TEST=readsyscall
   echo $FILE $BLOCK $TEST

#   drop_caches  --randomaccess
   $PERF ./fa -b ${BLOCK} --${TEST} -f ${FILE}
   exit 0
fi

# Uncomment for random access tests.
#ACCESS="--randomaccess"
echo $ACCESS

# Uncomment for creating a new file.
#CREATE="--createfile"
# Size in GB
#SIZE="-s 32"
#echo $CREATE ${SIZE}GB


#for TEST in readsyscall
#for TEST in readmmap readsyscall writesyscall
for TEST in readmmap readsyscall
#for TEST in writemmap writesyscall
do
    echo ${TEST}
#    for BLOCK in 512 1024 2048 4096 8192 16384
    for BLOCK in 4096 8192 16384
    do
	for i in {1..3}
	do
#	    drop_caches
	    ./fa -b ${BLOCK} --${TEST} -f ${FILE} --silent ${ACCESS} ${CREATE} ${SIZE}
	    # If the test needs to create the file each time, delete the file
	    # that the test just creates.
	    if [ -n "$CREATE" ]; then
		rm ${FILE}
	    fi
	done
    done
done

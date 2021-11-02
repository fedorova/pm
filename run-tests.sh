#!/bin/bash

#FILE=/mnt/data0/sasha/testfile
#FILE=/mnt/ssd/sasha/testfile
#FILE=/dev/pmem1
FILE=/mnt/pmem/sasha/testfile
#FILE=/dev/dax0.0
#FILE=/dev/nvme0n1
#FILE=/data/sasha/testfile
#FILE=/altroot/sasha/testfile

THREADS=8

if [ -z ${THREADS} ]; then
    THREADS=1
fi

echo $FILE

#PERF="perf record -g -e page-faults -e dTLB-load-misses -e LLC-load-misses  -e offcore_requests.all_data_rd -e offcore_response.all_code_rd.llc_miss.any_response -e kmem:* -e filemap:* -e huge_memory:* -e pagemap:* -e dtlb_load_misses.walk_completed_1g -e dtlb_load_misses.walk_completed_2m_4m -e dtlb_load_misses.walk_completed_4k -e dtlb_load_misses.walk_completed  -e dtlb_load_misses.walk_duration -e dtlb_load_misses.miss_causes_a_walk -e dtlb_load_misses.stlb_hit -e dtlb_load_misses.stlb_hit_2m -e dtlb_load_misses.stlb_hit_4k -e page-faults -e major-faults -e minor-faults -e cycles -e ext4:* -e vmscan:*"
#PERF="perf record -e cycles -e dTLB-loads -e dTLB-load-misses -e page-faults"

PERF="perf record -e mem_load_l3_miss_retired.remote_pmm -e mem_load_retired.local_pmm -e ocr.all_reads.pmm_hit_local_pmm.any_snoop -e ocr.pf_l1d_and_sw.pmm_hit_local_pmm.any_snoop -e ocr.pf_l2_data_rd.pmm_hit_local_pmm.any_snoop -e ocr.pf_l3_data_rd.pmm_hit_local_pmm.any_snoop"

PERF="perf stat -r 1 -e rEA01,rEA02,rEA04,rE300,rE700,rE001,rE401,r3708,r3820,rB701,rBB01 -e mem_load_l3_miss_retired.remote_pmm -e mem_load_retired.local_pmm -e ocr.all_reads.pmm_hit_local_pmm.any_snoop -e ocr.pf_l1d_and_sw.pmm_hit_local_pmm.any_snoop -e ocr.pf_l2_data_rd.pmm_hit_local_pmm.any_snoop -e ocr.pf_l3_data_rd.pmm_hit_local_pmm.any_snoop"

#  UNC_M_PMM_CMD1.ALL EventSel=EAH UMask=01H
#  UNC_M_PMM_CMD1.RD EventSel=EAH UMask=02H
#  UNC_M_PMM_CMD1.WR EventSel=EAH UMask=04H 
#  UNC_M_PMM_RPQ_INSERTS EventSel=E3H UMask=00H
#  UNC_M_PMM_WPQ_INSERTS EventSel=E7H UMask=00H
#  UNC_M_PMM_RPQ_OCCUPANCY.ALL EventSel=E0H UMask=01H
#  UNC_M_PMM_WPQ_OCCUPANCY.ALL EventSel=E4H UMask=01H
#  UNC_M2M_IMC_READS.TO_PMM EventSel=37H UMask=08H
#  UNC_M2M_IMC_WRITES.TO_PMM EventSel=38H UMask=20H
#  OCR.DEMAND_DATA_RD.PMM_HIT_LOCAL_PMM.ANY_SNOOP EventSel={B7H,BBH} UMask=01H
#  OCR.DEMAND_RFO.PMM_HIT_LOCAL_PMM.ANY_SNOOP EventSel={B7H,BBH} UMask=01H

drop_caches() {
    (echo 1) > /proc/sys/vm/drop_caches;
}

# Use the block below to run a single test while collecting profiling info
PROFILING_RUN=1
if [ ${PROFILING_RUN} = 1 ]
then
   echo "Doing a profiling run..."
   BLOCK=8192
   TEST=readmmap
   echo $FILE $BLOCK $TEST

#   drop_caches  --randomaccess
   $PERF -o $TEST.$THREADS.perf-stat ./fa -b ${BLOCK} --${TEST} -f ${FILE} -t ${THREADS}
   exit 0
fi

# Uncomment for random access tests.
#ACCESS="--randomaccess"

if [ "$ACCESS" = "--randomaccess" ]; then
    echo "random access"
else
    echo "sequential access"
fi

# Uncomment for direct I/O.
#DIRECTIO="-d"

if [ "$DIRECTIO" = "-d" ]; then
    echo "Using direct I/O."
fi

#SIZE="-s 32"
#echo $SIZE

#for TEST in readmmap readsyscall writemmap writesyscall
#for TEST in writemmap writesyscall
#for TEST in readmmap readsyscall writesyscall
for TEST in readmmap readsyscall
do
    echo ${TEST}
#    for BLOCK in 512 1024 2048 4096 8192 16384
#    for BLOCK in 4096 8192 16384
    for BLOCK in 8192
    do
    for i in {1}
    do
        for t in 1 2 4 8 16 32 64;
        do
#       drop_caches
        ./fa -b ${BLOCK} --${TEST} -f ${FILE} ${ACCESS} ${SIZE} -t $t --silent ${DIRECTIO}
        done
    done
    done
done

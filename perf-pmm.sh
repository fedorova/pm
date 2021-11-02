#!/bin/bash

OUTFILE=$1
pid=$2

echo "Will record PMM events for process" $pid "into file" $OUTFILE.$pid

perf stat -p $pid -o $OUTFILE.$pid -e rEA01,rEA02,rEA04,rE300,rE700,rE001,rE401,r3708,r3820,rB701,rBB01 -e mem_load_l3_miss_retired.remote_pmm -e mem_load_retired.local_pmm -e ocr.all_reads.pmm_hit_local_pmm.any_snoop -e ocr.pf_l1d_and_sw.pmm_hit_local_pmm.any_snoop -e ocr.pf_l2_data_rd.pmm_hit_local_pmm.any_snoop -e ocr.pf_l3_data_rd.pmm_hit_local_pmm.any_snoop 



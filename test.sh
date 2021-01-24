#!/bin/bash

# 测试不同数据量的FastFair的读写性能: YCSB uniform

LoadDatas="100000 1000000 10000000 100000000 1000000000"
DBs="fastfair pgm alex xindex"
WorkLoadsDir="./include/ycsb/workloads"
ExecDir="./build"
DBName="fastfair"
RecordCount="10000000"

loop = "0 1 2 3 4 5 6 7 8 9"
index = "0"
writerates="0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"
readrates="1.0 0.9 0.8 0.7 0.6 0.5 0.4 0.3 0.2 0.1 0"

function OperateDiffDB() {
    RecordCount="100000000"
    sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_test3.spec
    for index in $loop;
    do
        sed -r -i "s/insertproportiont=.*/insertproportion=${writerates[index]}/1" ${WorkLoadsDir}/workload_test3.spec
        sed -r -i "s/readproportion=.*/readproportion=${readrates[index]}/1" ${WorkLoadsDir}/workload_test3.spec
    done
}

OperateDiffDB
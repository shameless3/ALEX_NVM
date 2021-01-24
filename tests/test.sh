#!/bin/bash

# 测试不同数据量的FastFair的读写性能: YCSB uniform

LoadDatas="100000 1000000 10000000 100000000 1000000000"
DBs="fastfair pgm alex xindex"
WorkLoadsDir="./include/ycsb/workloads"
ExecDir="./build"
DBName="fastfair"
RecordCount="10000000"
writerates="0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9"
writerate="0"
readrate="1.0"

function OperateDiffDB() {
    RecordCount="100000000"
    sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_test3.spec
    for writerate in $writerates;
    do
        let readrate = 1 - $writerate
        sed -r -i "s/insertproportiont=.*/insertproportion=$writerate/1" ${WorkLoadsDir}/workload_test3.spec
        sed -r -i "s/readproportion=.*/readproportion=$readrate/1" ${WorkLoadsDir}/workload_test3.spec
    done
}

OperateDiffDB
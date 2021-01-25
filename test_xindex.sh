#!/bin/bash

# 测试不同数据量的FastFair的读写性能: YCSB uniform

LoadDatas="100000 1000000 10000000 100000000 1000000000"
DBs="fastfair pgm alex xindex"
WorkLoadsDir="./include/ycsb/workloads"
ExecDir="./build"
DBName="xindex"
RecordCount="10000000"

writerates="4 6 7 9 10"
writerate=0
readrate=10
function OperateDiffDB() {
    RecordCount="100000000"
    sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_test3.spec
    for writerate in $writerates
    do
        let readrate=10-writerate
        echo "readrate:$readrate    writerate:$writerate"
        sed -r -i "s/insertproportion=.*/insertproportion=$writerate/10" ${WorkLoadsDir}/workload_test3.spec
        sed -r -i "s/readproportion=.*/readproportion=$readrate/10" ${WorkLoadsDir}/workload_test3.spec
        ${ExecDir}/ycsb -db $DBName -threads 1 -P $WorkLoadsDir      
        sleep 60

    done
}

OperateDiffDB
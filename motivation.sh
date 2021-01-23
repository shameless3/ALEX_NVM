#!/bin/bash

# 测试不同数据量的FastFair的读写性能: YCSB uniform

LoadDatas="100000 1000000 10000000 100000000 1000000000"
DBs="fastfair pgm alex xindex"
WorkLoadsDir="./include/ycsb/workloads"
ExecDir="./build"
DBName="fastfair"
RecordCount="10000000"
writerates="0 0.1 0.2 0.3 0.4 0.5 0.6 0.7 0.8 0.9 1.0"
writerate="0"
readrate="1.0"

function OperateDiffLoad() {
    for RecordCount in $LoadDatas;
    do
        sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_read.spec
        sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_insert.spec
        ${ExecDir}/ycsb -db $DBName -threads 1 -P $WorkLoadsDir
        sleep 60
    done
}

function OperateDiffDB() {
    RecordCount="100000000"
    sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_test3.spec
    for writerate in $writerates;
    do
        readrate=1-writerate
        sed -r -i "s/recordcount=.*/insertproportion=$writerate/1" ${WorkLoadsDir}/workload_test3.spec
        sed -r -i "s/recordcount=.*/readproportion=$readrate/1" ${WorkLoadsDir}/workload_test3.spec
        for DBName in $DBs;
        do
            ${ExecDir}/ycsb -db $DBName -threads 1 -P $WorkLoadsDir      
            sleep 60
        done
    done
}

#OperateDiffLoad
sed -r -i "s/recordcount=.*/recordcount=$RecordCount/1" ${WorkLoadsDir}/workload_test3.spec
OperateDiffDB
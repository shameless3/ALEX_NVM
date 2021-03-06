#!/bin/bash

# 测试不同数据量的FastFair的读写性能: YCSB uniform

LoadDatas="100000 1000000 10000000 100000000 1000000000"
DBs="fastfair pgm alex xindex"
WorkLoadsDir="./include/ycsb/workloads"
InsertRatioDir="./include/ycsb/insert_ratio"
ExecDir="./build"
DBName="fastfair"
RecordCount="10000000"

writerates="0 1 2 3 4 5 6 7 8 9 10"
writerate=0
readrate=10

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
    for writerate in $writerates
    do
        let readrate=10-writerate
        echo "readrate:$readrate    writerate:$writerate"
        sed -r -i "s/insertproportion=.*/insertproportion=$writerate/10" ${WorkLoadsDir}/workload_test3.spec
        sed -r -i "s/readproportion=.*/readproportion=$readrate/10" ${WorkLoadsDir}/workload_test3.spec
        for DBName in $DBs;
        do
            ${ExecDir}/ycsb -db $DBName -threads 1 -P $WorkLoadsDir      
            sleep 60
        done
    done
}

function OperateDiffDB2() {
    for DBName in $DBs;
    do
        ${ExecDir}/ycsb -db $DBName -threads 1 -P $InsertRatioDir 
        sleep 60
    done
}

# OperateDiffLoad
# OperateDiffDB
# OperateDiffDB2
# ${ExecDir}/ycsb -db pgm -threads 1 -P $InsertRatioDir
${ExecDir}/ycsb -db xindex -threads 1 -P $InsertRatioDir
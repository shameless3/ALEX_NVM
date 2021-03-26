#!/bin/bash

# 测试不同数据量的FastFair的读写性能: OSM asia

ExecDir="./build"
readrates="0 1 2 3 4 5 6 7 8 9 10"

function OperateDiffDB(){
    for readrate in $readrates
    do 
        echo "read rate:$readrate"
        #${ExecDir}/osm_test -db all -read $readrate
        #${ExecDir}/osm_test -db fastfair -read $readrate
        #${ExecDir}/osm_test -db pgm -read $readrate
        #${ExecDir}/osm_test -db alex -read $readrate
        #${ExecDir}/osm_test -db xindex -read $readrate
        ${ExecDir}/osm_test -db learngroup -read $readrate        
    done
}

OperateDiffDB
#${ExecDir}/osm_test -db pgm -read 10

# all 指 alex fastfair xindex pgm
# ${ExecDir}/osm_test -db all -read 8
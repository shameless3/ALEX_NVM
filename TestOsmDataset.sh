#!/bin/bash

# 测试不同数据量的FastFair的读写性能: OSM asia

ExecDir="./build"

# ${ExecDir}/osm_test -db alex -read 8
# ${ExecDir}/osm_test -db fastfair -read 8
# ${ExecDir}/osm_test -db xindex -read 8
# ${ExecDir}/osm_test -db pgm -read 8

#all 指 alex fastfair xindex pgm
${ExecDir}/osm_test -db all -read 8
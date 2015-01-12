#!/bin/bash

####
## FIXME-marina: Careful! This script does not support the scheduling of jobs
## to more than 1 node!
####

if [ "$#" -ne "3" ]; then
    echo "Usage: $0 <job id> <nodes> <cpus>";
    exit -1;
fi;

jobid=$1
nodeNames=$2
cpuStr=$3

# Allocation file that needs to be written
ALLOCOUT=/tmp/alloc_out_${jobid}.txt
TOPOLUT=server_topo_lut.txt

node=`cat $TOPOLUT | grep "\"$nodeNames\""`
echo "Node is: $node and cpus: $cpuStr";
echo "[$node,$cpuStr]" > $ALLOCOUT ;

exit 0;


#!/bin/bash

## Dummy allocator does only a very stuping round-robin allocation 
## based on jobid (works only w/slurm simulator, JobId < numservers < 20, 
## and 1 partition).
## It just subtracts the initial job id to assign a server
## This should be in accordance to slurm.conf or nothing will work!
rackname="rack"
servername="s"

PREVALLOC=$1
CURRJOB=$2
ALLOCOUT=$3
ALLOCBMP=$4
JOBID=$5

nodealloc=$(( $JOBID - 1000 ))
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 :Dummy allocator working...";
echo "[\"${rackname}1\",\"$servername$nodealloc\",256]";
echo "[\"${rackname}1\",\"$servername$nodealloc\",256]" > $ALLOCOUT;

#Convert ALLOCOUT into bitmap for slurm
# node,numcores --> one per line
cat $ALLOCOUT | sed -e 's#]##g' | sed -e 's#"##g' | awk -F"," '{print $2","$3}' | grep '^s' > $ALLOCBMP ;

exit 0;


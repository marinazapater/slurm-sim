#!/bin/bash

nonfree=$1
newjobpwr=$2
jobid=$3

SLURM_SUCCESS=0
SLURM_ERROR=-1
# Previous allocation was filled-in by DCSim in previous iteration
# (keeps track of all data, and was launched first)
PREVALLOC=/tmp/prev_alloc.txt
# Current job parameters
CURRJOB=/tmp/param_job_${jobid}.txt
# Allocation provided by algorithm
ALLOCOUT=/tmp/alloc_out_${jobid}.txt
ALLOCBITMAP=/tmp/alloc_bitmap.txt

SOCKHOST=localhost
SOCKPORT=1234
#LOG_FILE=/tmp/sim_sbatch-ext-alloc.log

POLICY="slurm_default"

# MAIN program execution
#-----------------------
#exec >> $LOG_FILE;

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : External allocator working" ;
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Non-free nodes: $1";
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : JobPower: $2";
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : JobId: $3";

#Halt slurm-sim execution
echo "halt" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ;  
echo "[$newjobpwr]" > $CURRJOB ;

# Callingn allocator
echo "Calling allocator...";
./dummy_allocator.sh $PREVALLOC $CURRJOB $ALLOCOUT ${jobid}

#Convert ALLOCOUT into bitmap for slurm
# node,numcores --> one per line
cat $ALLOCOUT | sed -e 's#]##g' | sed -e 's#"##g' | awk -F"," '{print $2","$3}' > $ALLOCBITMAP ;

# Continue slurm-sim execution 
echo "continue" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ; 

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Fi";

# We exit with slurm_error because we did not apply any policy
exit $SLURM_ERROR;


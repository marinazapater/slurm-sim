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
ALLOCBITMAP=/tmp/alloc_btm_${jobid}.txt

SOCKHOST=localhost
SOCKPORT=1234
#LOG_FILE=/tmp/sim_sbatch-ext-alloc.log

POLICY="slurm_default"

# MAIN program execution
#-----------------------
#exec >> $LOG_FILE;

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : External allocator working" ;
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : Non-free nodes: $1";
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : JobPower: $2";
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : JobId: $3";

#Halt slurm-sim execution
echo "halt" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ;  
echo "[$newjobpwr]" > $CURRJOB ;

# Callingn allocator
echo "Calling allocator...";
./dummy_allocator.sh $PREVALLOC $CURRJOB $ALLOCOUT $ALLOCBITMAP ${jobid}
rc=$?

# Continue slurm-sim execution 
echo "Signaling continue to slurm-sim";
echo "continue" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ; 

if [[ $rc != $SLURM_SUCCESS ]] ; then
    echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : error during allocation.";
    exit $SLURM_ERROR;
fi
# We exit with slurm_error because we did not apply any policy
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : $0 : Succesful allocation";
exit $SLURM_SUCCESS;


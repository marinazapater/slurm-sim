#!/bin/bash

if [ "$#" -ne "3" ]; then
    echo "Usage: $0 <job id> <time> <message type: [jobbegin|jobend]>";
    exit -1;
fi;

jobid=$1
jobtime=$2
type=$3

SLURM_SUCCESS=0
SLURM_ERROR=-1

# Previous allocation was filled-in by DCSim in previous iteration
# (keeps track of all data, and was launched first)
PREVALLOC=/tmp/prev_alloc.txt
# Current job parameters
CURRJOB=/tmp/param_job_${jobid}.txt
# Allocation provided by algorithm
ALLOCOUT=/tmp/alloc_out_${jobid}.txt

if [ "x$SLURM_ENV" == "x" ]; then
    echo "Environmental variables not set. Please run shenv in slurm-sim vm";
    exit -1;
fi;

LOG_FILE=/tmp/sim_sbatch-dcsim-caller.log
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : DCsim caller working" ;
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Stopping slurm-sim simulator" ;
echo "halt" | nc -v -w 0 -u $SLURMSOCKHOST $SLURMSOCKPORT ;  

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Synching files... (sending allocation to DCsim)";
rsync $CURRJOB $DCSIMUSER@$DCSIMHOST:$CURRJOB ;
rsync $ALLOCOUT $DCSIMUSER@$DCSIMHOST:$ALLOCOUT ;

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Calling dcsim for job $type" ;
echo "${type};${jobtime};${jobid}";
echo "${type};${jobtime};${jobid}" | nc -v -w 0 -u $DCSIMHOST $DCSIMPORT;

sleep 2;
#TODO-marina: this needs to be implemented!!
# if [ "$type" == "jobend" ]; then
#     echo "Job $jobid requires cleanup. Will wait for completion";
#     end=0
#     while [ $end -eq 0 ] ; do
#         if cat $CURRJOB | grep 'done' ; then
            
#         fi;
#     done ;
# fi ;

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Signaling slurm sim to continue";
echo "continue" | nc -v -w 0 -u $SLURMSOCKHOST $SLURMSOCKPORT ;  

exit $SLURM_SUCCESS ;


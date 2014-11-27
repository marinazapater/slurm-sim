#!/bin/bash

SLURM_SUCCESS=0
SLURM_ERROR=-1

SOCKHOST=localhost
SOCKPORT=1234
LOG_FILE=/tmp/sim_sbatch-ext-alloc.log

POLICY="slurm_default"

exec >> $LOG_FILE;

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : External allocator working" ;
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Non-free nodes: $1";
echo "`date +\"%Y-%m-%d %H:%M:%S\"` : JobPower: $2";
echo "halt" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ;  
echo "Retrieving data from slurmDBD..." ;
sleep 1;
echo "continue" | nc -v -w 0 -u $SOCKHOST $SOCKPORT ; 

echo "`date +\"%Y-%m-%d %H:%M:%S\"` : Fi";

exit $SLURM_ERROR;


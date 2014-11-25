#!/bin/bash

SLURM_SUCCESS=0
SLURM_ERROR=-1

LOG_FILE=/tmp/sim_sbatch-ext-alloc.log

POLICY="slurm_default"

exec >> $LOG_FILE;

echo "`date +%Y-%m-%d %H:%M:%S` : External allocator working" ;
echo "`date +%Y-%m-%d %H:%M:%S` : Arguments: $1";
sleep 2;

echo "`date +%Y-%m-%d %H:%M:%S` : Fi";

exit $SLURM_ERROR;


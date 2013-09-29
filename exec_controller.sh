#!/bin/bash

`LD_LIBRARY_PATH=$1/slurm_varios/lib/slurm LD_PRELOAD=libslurm_sim.so $1/slurm_programs/sbin/slurmctld -f $1/slurm_conf/slurm.conf -D 2&> slurmctl.log`;

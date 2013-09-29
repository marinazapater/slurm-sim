#!/bin/bash

`SLURM_CONF=$1/slurm_conf/slurm.conf LD_LIBRARY_PATH=$1/slurm_varios/lib/slurm LD_PRELOAD=libslurm_sim.so $1/slurm_programs/sbin/slurmd -D 2&> slurmd_sim.log`;

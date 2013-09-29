#!/bin/bash
# @ class = class_a
# @ job_name = testF1-1
# @ account = regular
# @ output = /tmp/testF1-1.out
# @ error = /tmp/testF1-1.err
# @ total_tasks = 4
# @ wall_clock_limit = 00:10:00

export OUTDIR=`pwd`/OUTPUT

srun hostname


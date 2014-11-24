#!/bin/bash

SERVER="tester@slurm-vm2.lsi.die"
PATH="/home/tester/SIMULATOR/sim_test_dir/slurm/src/plugins/"

echo "Deploying code to $SERVER" ;
rsync -avP * $SERVER:/$PATH ;


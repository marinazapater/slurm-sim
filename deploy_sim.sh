#!/bin/bash

SERVER=tester@slurm-vm2.lsi.die
SIMPATH=/home/tester/SIMULATOR/sim_test_dir/
SIMLIBPATH=/home/tester/SIMULATOR/sim_test_dir/slurm/src/simulation_lib/

echo "Deploying simulator code to $SERVER" ;

rsync -avP launch_new_simulation.sh $SERVER:/$SIMPATH ;
rsync -avP exec_sim.pl $SERVER:/$SIMPATH ;
rsync -avP mysql-scripts $SERVER:/$SIMPATH;

rsync -avP simulation_lib/*.[c,h] $SERVER:/$SIMLIBPATH ; 

cd plugin/ && ./deploy.sh && cd .. ;


#!/bin/sh

SERVER=tester@slurm-vm2.lsi.die
SIMPATH=/home/tester/SIMULATOR/sim_test_dir/
PLUGINPATH=$SIMPATH/slurm/src/plugins/

echo "Deploying plugin code to $SERVER" ;

rsync -avP * --exclude=*.sh $SERVER:/$PLUGINPATH ;
rsync -avP *_allocator.sh $SERVER:/$SIMPATH ;
rsync -avP *_allocator.py $SERVER:/$SIMPATH ;
rsync -avP dcsim_caller.sh $SERVER:/$SIMPATH ;


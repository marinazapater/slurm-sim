#!/bin/sh

SERVER=tester@slurm-vm2.lsi.die
SIMPATH=/home/tester/SIMULATOR/sim_test_dir/
PLUGINPATH=$SIMPATH/slurm/src/plugins/

echo "Deploying code to $SERVER" ;

rsync -avP * --exclude=external_allocator.sh --exclude=deploy.sh $SERVER:/$PLUGINPATH ;
rsync -avP external_allocator.sh $SERVER:/$SIMPATH ;


#!/bin/bash

echo "### Start patching Slurm installation with simulator changes..." ;
cd slurm-patches && ./patch_slurm.sh && cd - ;

echo "### Copying simulation_lib to its destination" ;
cp -r simulation_lib ./slurm/src/

echo "### Generating trace and user files"
echo "" > rsv.trace;


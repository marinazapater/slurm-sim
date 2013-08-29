#!/bin/bash

echo "--- Patching slurm/config.h.in"

echo "/* Define to 1 if running slurm simulator */" >> ../slurm/config.h.in ;
echo "#undef SLURM_SIMULATOR" >> ../slurm/config.h.in ;

echo "--- Patching configure.ac"
#patch ../slurm/configure.ac -i ./configure.ac.patch ;

echo "--- Copying monfig to slurm folder"
cp ./my_config ../slurm/


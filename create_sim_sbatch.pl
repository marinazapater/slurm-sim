#!/usr/bin/perl

$SIMDIR=`pwd`;

chomp($SIMDIR);

$install_dir=$SIMDIR;

# We need to create a wrapper for executing sbatch preloading simulation library

open OUT,"+>","slurm_programs/bin/sim_sbatch" or die("sim_sbatch creation error");

print OUT "#!/bin/bash\n";

print OUT "PARAMS=`echo \$@`\n";
print OUT "DATE=`date +%s`\n";

print OUT "# Just for debug\n";
print OUT "echo \"LD_LIBRARY_PATH=$install_dir/slurm_varios/lib/slurm LD_PRELOAD=libslurm_sim.so $install_dir/slurm_programs/bin/sbatch\" \$PARAMS > /tmp/sim_sbatch-\$DATE\n";

print OUT "RES=`LD_LIBRARY_PATH=$install_dir/slurm_varios/lib/slurm LD_PRELOAD=libslurm_sim.so $install_dir/slurm_programs/bin/sbatch \$PARAMS > /tmp/sbatch.out`\n";

close OUT;

$chmod_command="chmod 755 $install_dir/slurm_programs/bin/sim_sbatch";
print "Executing $chmod_command\n";
system($chmod_command);

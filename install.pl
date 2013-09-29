#!/usr/bin/perl

$SIMDIR=`pwd`;

chomp($SIMDIR);

$install_dir=$SIMDIR;

$config_command = "cd slurm;./configure --exec-prefix=$install_dir/slurm_programs --bindir=$install_dir/slurm_programs/bin --sbindir=$install_dir/slurm_programs/sbin --datadir=$install_dir/slurm_varios/share --includedir=$install_dir/slurm_varios/include --libdir=$install_dir/slurm_varios/lib --libexecdir=$install_dir/slurm_varios/libexec --localstatedir=$install_dir/slurm_varios --sharedstatedir=$install_dir/slurm_varios --mandir=$install_dir/slurm_varios/man --infodir=$install_dir/slurm_varios/share/info --prefix=$install_dir/slurm_programs --sysconfdir=$install_dir/slurm_conf --localstatedir=$install_dir/slurm_varios/var/ --enable-pam --with-proctrack --with-ssl=$install_dir/slurm_varios/lib/slurm --without-munge --enable-front-end --with-mysql-config=/usr/bin/ --enable-simulator 2> slurm_configure.log";

print "Executing:\n $config_command\n";
system($config_command);

print "\nNow compiling ... (It will take some minutes)\n";
system("cd slurm;make 2> make.log");
print ("... and installing ...\n");
system("cd slurm;make install 2> make-install.log");


# Let's check compilation and installation was successful
print ("Let's check compilation and installation was successful...\n");

if(-e "slurm_programs/sbin/slurmctld"){
    print "I got a slurmctld binary!\n";
}
else{
    die("Something was wrong during slurm building\n");
}

if(-e "slurm_programs/bin/sim_mgr"){
    print "I got a sim_mgr binary!\n";
}
else{
    die("Something was wrong during slurm building\n");
}

print ("It seems fine.\n");


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

# After slurm has been compiled and installed, let's take the sim_mgr program here

system("objdump -t slurm_programs/sbin/slurmctld | grep _slurmctld_rpc_mgr | awk '{print \$1 \" # _slurmctld_rpc_mgr\" '} > rpc_threads.info");
system("objdump -t slurm_programs/sbin/slurmctld | grep _slurmctld_signal_hand | awk '{print \$1 \" # _slurmctld_signal_hand\" '} >> rpc_threads.info");
system("objdump -t slurm_programs/sbin/slurmctld | grep _agent | awk '{print \$1 \" # _agent\" '} >> rpc_threads.info");

system("cp slurm/src/simulation_lib/list_trace .");

print "\There are some parameters at slurm_conf/slurm.con to adjust before slurm can run:\n";
print "\n\t\tSlurmUser\n";
print "\n\t\tSlurmdUser\n";
print "\n\t\tControlMachine\n";
print "\n\t\tControlAddr\n";
print "\n\t\tClusterName\n";
print "\n\t\tFrontendName\n";

print "\n\t Also check slurm_conf/slurm.nodes and set NodeHostName and NodeAddr if they are wrong\n";
print "\n\nDO NOT FORGET IT!\n\n";

print "\There are some parameters at slurm_conf/ to adjust before slurmdbd can run::\n";
print "\n\t\tStorageUser\n";
print "\n\t\tStoragePass\n";
print "\n\t\tSlurmUser\n";
print "\n\nDO NOT FORGET IT!\n\n";



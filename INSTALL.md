This document explains the steps needed to launch the slurm simulator.

Along with a slurm version patched with simulator code, you got:

- an installation script: install.pl
- a mysql slurm database dump: perfdevel
- a file with users used by slurm simulator: users.sim
- a job trace file: test.trace
- a reservation trace file: rsv.trace
- a main script for launching the simulation: exec_sim.pl
- several other scripts used by exec_sim.pl


So you got all the necessary for running the simulator with a specific cluster.
The environment for using the simulator is a bit complex, so first use the
file given: mysql_perfdevel_slurm, users.sim, test.trace, etc. 

Afterwards, you can run the simulator with your own data.


1) Copy the sim_test.tar.bz2 file to whatever you want to install the simulator. 
   We will use SIM_DIR from now on.

2) bunzip2 sim_test.tar.bz2
3) tar xvf sim_test.tar
4) cd sim_test_dir
5) ./install.pl
   This script buils slurm and creates configuration files.
   The final step is installing slurm.
   If everything was fine you got some messages for adding some fields to slurm.conf and slurmdbd.conf

   If you are using a VM and you are using the user 'tester', you just have to be worried about 
   the hostname. If your VM Linux instance hostname is 'ubuntu', you have nothing to do.

   In other case:
   Use the same user making the installation for running slurmcltd and slurmd.
   The cluster name needs to be "perfdevel"
   Adds the user making the installation to the file users.sim. This user needs to be at /etc/passwd.
   Make sure there is not another user at users.sim with the same uid
   Simulator uses slurm in frontend mode. Just use same machine name for 
   ControlMachine=
   ControlAddr=
   FrontendName=

   modify slurm.nodes using your hostname ("ubuntu"?)  for NodeAddr and NodeHostName


6) Make sure you mysql database is running.
   You need to create a user with some privileges:

	mysql> create user 'slurm'@'localhost' identified by 'ihatelsf'
	mysql> grant all privileges on *.* to 'slurm'@'localhost' with grant option


   Once here you can start the slurmdbd program placed at SIM_DIR/slurm_programs/sbin

   slurmdbd creates the slurm_acct_db database. 

7) Now dumping perfdevel slurm database. Use this by now. 

   mysql slurm_acct_db < SIM_DIR/mysql_perfdevel_slurm 

   Now you should have perfdevel tables in slurm_acct_db

   You should be able to run 
   
   SIM_DIR/slurm_programs/bin/sacctmgr show qos
   SIM_DIR/slurm_programs/bin/sacctmgr show assoc

   If you can not see the assocs, restart the slurmdbd.
   If not, you have a problem with the installation. Not a simulation problem yet.

   
8) Created the slurm.key and slurm.cert files at SIM_DIR/slurm_conf directory

   cd SIM_DIR

   openssl genrsa -out slurm_conf/slurm.key

   openssl rsa -in slurm_conf/slurm.key -out slurm_conf/slurm.cert


9) Check slurmctld and slurmd can be executed with current configuration

  cd SIM_DIR

  slurm_programs/sbin/slurmctld -D

  slurm_programs/sbin/slurmd -D
   
  Fix the problems till both daemons can be executed


10) Now you are ready for running the simulator.

  Stops the slurmctld and slurmd if running

  cd SIM_DIR

  ./exec_sim.pl SIM_DIR 100

  This scripts calls sim_mgr, which calls slurmctld and slurmd.
  After 10 seconds or so do:

  ps axl | grep slurm

  If you can not see sim_mgr, slurmctld and slurmd, there's a problem.
  Check exec_sim.log, sim_mgr.log, slurmcltd.log and slurmd_sim.log at SIM_DIR

11)

	It's running!

	Check SIM_DIR/slurm_varios/acct/job_comp.log

	you should see jobs finishing under simulation 

	Use:

	list_trace program at SIM_DIR directory for getting a list of launched jobs

	
	Report any problem to alejandro.lucero@bsc.es




#!/usr/bin/perl

my $WORKDIR=$ARGV[0];
my $TOTAL_JOBS=$ARGV[1];
my $SERVER=$ARGV[2];
my $PORT=$ARGV[3];

my $logfile="$WORKDIR/exec_sim.log";

#open LOG,">$logfile";
open LOG,"|tee $logfile";

print LOG "Launching sim_mgr...$WORKDIR/sim_mgr\n";
system("cd $WORKDIR");

system("SLURM_CONF=$WORKDIR/slurm_conf/slurm.conf SLURM_PROGRAMS=$WORKDIR/slurm_programs/bin ./sim_mgr 0 $SERVER $PORT &");

sleep(5);
print LOG "Launching slurmctld...\n";
system("$WORKDIR/exec_controller.sh $WORKDIR &");

sleep(5);
print LOG "Launching slurmd...\n";
system("$WORKDIR/exec_slurmd.sh $WORKDIR &");

sleep(10);

# Let's see which addresses have main slurm functions including plugins
open PS,"ps axl | grep -v grep| grep slurmctld|";

while (<PS>){

	print LOG "$_\n\n";
	if(/(\d+)(\s+)(\d+)(\s+)(\d+)/){
		print LOG "Getting maps for process ID $5\n";
		system("more /proc/$5/maps > slurmctld.maps");
	}
}

close PS;

print LOG "Waiting...\n";
while(1){
	$jobs_completed=`more $WORKDIR/slurm_varios/acct/jobcomp.log | grep JobId | wc -l`;
	if(($jobs_completed == $TOTAL_JOBS) || ($jobs_completed > $TOTAL_JOBS)){
		last;
	}
	print LOG "Just $jobs_completed jobs completed of $TOTAL_JOBS. Sleeping\n";
	sleep(60);
}

print LOG "Ok. We have $jobs_completed completed jobs\n";

sleep 5;
system("sync");
close PS;
print LOG "Killing simulation processes...\n\n";

# Killing sim_mgr with SIGINT signal

`killall sim_mgr -SIGINT $5`;


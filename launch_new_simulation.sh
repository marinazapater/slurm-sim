#!/bin/bash

if [ $# -eq 1 ] ; then
    allocresults=$1_alloc.txt
    statsresults=$1_stats.txt
else
    allocresults=allocresults.txt
    statsresults=statsresults.txt
fi;

echo -n "Resetting accounting files... ";
./reset.sh
echo "Done" ;
echo -n "Deleting old log files...";
rm /tmp/sim_sbatch-*
echo "Done" ;
sleep 2 ;

num=`./list_trace | wc -l`
if [ $num -lt 2 ]; then
	echo "FATAL: No jobs in ./list_trace" ;
	exit ;
fi;

numjobs=$(( $num - 2 ))
echo "Number of jobs to simulate: $numjobs"

echo "Launching exec_sim.pl";
./exec_sim.pl . $numjobs 

echo "Retrieving accounting results. Storing in results.txt" ;

mysql -u root --password=slurm < ./mysql-scripts/get_simulator_allocation.mysql_script > $allocresults ; 
mysql -u root --password=slurm < ./mysql-scripts/get_simulator_statistics.mysql_script > $statsresults ; 

echo "Exiting..." ;

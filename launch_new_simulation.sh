#!/bin/bash

extalloc=false
servername=localhost
serverport=1234

allocresults=allocresults.txt
statsresults=statsresults.txt

if [ $# -eq 1 ] ; then
    if [ $1 == "ext" ] ; then
        extalloc=true
    else
        allocresults=$1_alloc.txt
        statsresults=$1_stats.txt
    fi ;
fi;

if [ "x$SLURM_ENV" == "x" ]; then
    echo "Environmental variables not set. Please run shenv in slurm-sim vm";
    exit -1;
fi;

./test_env_vars.sh
if [ "$?" -lt 0 ] ; then
    echo "Error when testing env variables. Exiting.";
    exit -1;
fi;

echo -n "Resetting accounting files... ";
./reset.sh
echo "Done" ;
echo -n "Deleting old log files...";
rm /tmp/sim_sbatch-*
rm /tmp/alloc_*.txt
rm /tmp/param_*.txt
echo "Done" ;
sleep 2 ;

echo "Copying files to simulation folder";
cp ./slurm/src/simulation_lib/list_trace .
cp ./slurm/src/simulation_lib/sim_mgr .

num=`./list_trace | wc -l`
if [ $num -lt 2 ]; then
	echo "FATAL: No jobs in ./list_trace" ;
	exit ;
fi;

numjobs=$(( $num - 2 ))
echo "Number of jobs to simulate: $numjobs"

echo "Launching exec_sim.pl";
./exec_sim.pl . $numjobs $servername $serverport 

echo "Retrieving accounting results. Storing in results.txt" ;

mysql -u root --password=slurm < ./mysql-scripts/get_simulator_allocation.mysql_script > $allocresults ; 
mysql -u root --password=slurm < ./mysql-scripts/get_simulator_statistics.mysql_script > $statsresults ; 

echo "Signaling exit";
echo "exit;" | nc -v -w 0 -u $DCSIMHOST $DCSIMPORT;

echo "Exiting..." ;

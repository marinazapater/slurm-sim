#!/usr/bin/perl

system("objdump -t ../slurmctld/slurmctld | grep _slurmctld_rpc_mgr | awk '{print \$1 \" # _slurmctld_rpc_mgr\" '} > rpc_threads.info");
system("objdump -t ../slurmctld/slurmctld | grep _slurmctld_signal_hand | awk '{print \$1 \" # _slurmctld_signal_hand\" '} >> rpc_threads.info");
system("objdump -t ../slurmctld/slurmctld | grep _agent | awk '{print \$1 \" # _agent\" '} >> rpc_threads.info");


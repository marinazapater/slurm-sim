## Slurm-sim running instructions
==================================

Once you've gone through the install instructions, do the following:
* Run slurmdbd in slurm_programs/sbin/slurmdbd
* Configure slurm.conf and slurm.nodes in slurm_conf
* Remove test.trace
* Create new trace: `slurm/src/simulation_lib/trace_build_from_file
  --jobsfile=path to jobsfile`
* You can list the trace with `./list_trace`
* Launch simulator with: `./launch_new_simulation.sh`


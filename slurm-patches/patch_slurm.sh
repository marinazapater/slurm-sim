#!/bin/bash

echo "--- Patching slurm/config.h.in"

echo "/* Define to 1 if running slurm simulator */" >> ../slurm/config.h.in ;
echo "#undef SLURM_SIMULATOR" >> ../slurm/config.h.in ;

echo "--- Patching configure.ac"
patch ../slurm/configure.ac -i ./configure.ac.patch ;

echo "--- Copying myconfig to slurm folder"
cp ./my_config ../slurm/

echo "--- Patching slurm/src/common/forward.c"
patch ../slurm/src/common/forward.c -i ./forward.c.patch ;

echo "--- Patching slurm/src/common/slurm_jobacct_gather.c"
patch ../slurm/src/common/slurm_jobacct_gather.c -i ./slurm_jobacct_gather.c.patch ;

echo "--- Patching slurm/src/common/slurm_protocol_api.c"
patch ../slurm/src/common/slurm_protocol_api.c -i ./slurm_protocol_api.c.patch ;

echo "--- Patching slurm/src/common/slurm_protocol_defs.c"
patch ../slurm/src/common/slurm_protocol_defs.c -i ./slurm_protocol_defs.c.patch ;

echo "--- Patching slurm/src/common/slurm_protocol_defs.h"
patch ../slurm/src/common/slurm_protocol_defs.h -i ./slurm_protocol_defs.h.patch ;

echo "--- Patching slurm/src/common/slurm_protocol_pack.c"
patch ../slurm/src/common/slurm_protocol_pack.c -i ./slurm_protocol_pack.c.patch ;

echo "--- Patching slurm/src/Makefile.in"
patch ../slurm/src/Makefile.in -i ./Makefile.in.patch ;

echo "--- Patching slurm/src/plugins/accounting_storage/slurmdbd/accounting_storage_slurmdbd.c"
patch ../slurm/src/plugins/accounting_storage/slurmdbd/accounting_storage_slurmdbd.c -i ./accounting_storage_slurmdbd.c.patch ;

echo "--- Patching slurm/src/plugins/sched/backfill/backfill.c"
patch ../slurm/src/plugins/sched/backfill/backfill.c -i ./backfill.c.patch ;

echo "--- Patching slurm/src/salloc/salloc.c "
patch ../slurm/src/salloc/salloc.c  -i ./salloc.c.patch ;

echo "--- Patching slurm/src/slurmctld/agent.c"
patch ../slurm/src/slurmctld/agent.c -i ./agent.c.patch ;

echo "--- Patching slurm/src/slurmctld/controller.c"
patch ../slurm/src/slurmctld/controller.c -i ./controller.c.patch ;

echo "--- Patching slurm/src/slurmctld/job_mgr.c"
patch ../slurm/src/slurmctld/job_mgr.c -i ./job_mgr.c.patch ;

echo "--- Patching slurm/src/slurmctld/job_scheduler.c"
patch ../slurm/src/slurmctld/job_scheduler.c -i ./job_scheduler.c.patch ;

echo "--- Patching slurm/src/slurmctld/node_scheduler.c"
patch ../slurm/src/slurmctld/node_scheduler.c -i ./node_scheduler.c.patch ;

echo "--- Patching slurm/src/slurmctld/proc_req.c"
patch ../slurm/src/slurmctld/proc_req.c -i ./slurmctld_proc_req.c.patch ;

echo "--- Patching slurm/src/slurmd/slurmd/req.c"
patch ../slurm/src/slurmd/slurmd/req.c -i ./req.c.patch ;

echo "--- Patching slurm/src/slurmd/slurmd/slurmd.c"
patch ../slurm/src/slurmd/slurmd/slurmd.c -i ./slurmd.c.patch ;

echo "--- Patching slurm/src/slurmdbd/proc_req.c"
patch ../slurm/src/slurmdbd/proc_req.c -i ./slurmdbd_proc_req.c.patch ;


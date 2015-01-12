/*****************************************************************************\
 *  extern_alloc.h - node selection plugin supporting the call to an external
 *  allocation module
 *****************************************************************************\
 *
 *  Go to the commented source code to understand how this works ;)
 *
 *****************************************************************************
 *  Written by Marina Zapater <marina@die.upm.es>, who borrowed heavily
 *  from select/cons_res
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.

\*****************************************************************************/

#ifndef _CR_EXTERN_ALLOC_H
#define _CR_EXTERN_ALLOC_H

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "slurm/slurm.h"
#include "slurm/slurm_errno.h"

#include "src/common/list.h"
#include "src/common/log.h"
#include "src/common/node_select.h"
#include "src/common/pack.h"
#include "src/common/slurm_protocol_api.h"
#include "src/common/xassert.h"
#include "src/common/xmalloc.h"
#include "src/common/xstring.h"
#include "src/common/slurm_resource_info.h"
#include "src/slurmctld/slurmctld.h"

#include "select_cons_res_ext.h"
#include "job_test_ext.h"

#define EXTALLOC_TASK_BEGIN 1
#define EXTALLOC_TASK_END   0

int extalloc_print_job_bitmap ( bitstr_t *bitmap );

uint16_t *external_allocator (struct job_record *job_ptr, uint32_t min_nodes,
                                  uint32_t max_nodes, uint32_t req_nodes,
                                  bitstr_t *node_map, uint32_t cr_node_cnt,
                                  bitstr_t *core_map,
                                  struct node_use_record *node_usage,
                                  uint16_t cr_type, bool test_only);

void print_slurm_alloc (struct job_record *job_ptr, bitstr_t *node_map,
                        uint16_t *cpu_cnt, uint32_t cr_node_cnt);
    
uint16_t call_dcsim (struct job_record *job_ptr, bitstr_t *node_map,
                     int begin, time_t now);

#endif //_CR_EXTERN_ALLOC_H

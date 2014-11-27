/*****************************************************************************\
 *  extern_alloc.c - node selection plugin supporting the call to an external
 *  allocation module
 *****************************************************************************\
 *
 *  This is how it goes:
 *
 *****************************************************************************
 *  Written by Marina Zapater <marina@die.upm.es>, who borrowed heavily
 *  from select/cons_res
 *
 *  This file is part of SLURM, a resource management program.
 *  For details, see <http://www.schedmd.com/slurmdocs/>.
 *  Please also read the included file: DISCLAIMER.

\*****************************************************************************/

#include "extern_alloc.h"
#include "slurm/slurm_errno.h"

int extalloc_print_job_bitmap ( bitstr_t *bitmap )
{
    char *str; 
    int numbits;
    if (bitmap) {
        numbits = bit_size(bitmap);
        str = xmalloc(numbits*sizeof(char));
        bit_fmt(str, numbits*sizeof(char), bitmap);
        info("  const_res_ext: print_job_bitmap - numbits is: %d, bitmap is %s", numbits, str);
        xfree(str);
    } else {
        info("  const_res_ext: extern_alloc - [no row_bitmap]");
    }

    return SLURM_SUCCESS;
}

uint16_t *external_allocator (struct job_record *job_ptr, uint32_t min_nodes,
                              uint32_t max_nodes, uint32_t req_nodes,
                              bitstr_t *node_map, uint32_t cr_node_cnt,
                              bitstr_t *core_map,
                              struct node_use_record *node_usage,
                              uint16_t cr_type, bool test_only)
{
    int rc = SLURM_ERROR;
    char *nodes;
    char str[1000];
    uint16_t *cpu_cnt, *cpus = NULL;
    uint32_t start, n, a;
    
    extalloc_print_job_bitmap(node_map);
    extalloc_print_job_bitmap(core_map);
    nodes = bitmap2node_name(node_map);
    info("external_allocator: non-free nodes %s", nodes);
    info("external_allocator: job comment %s", job_ptr->comment);
    sprintf(str, "./external_allocator.sh %s %s", nodes, job_ptr->comment);
    rc = system(str);
   
    /* if successful, sync up the core_map with the node_map, and */
	/* create a cpus array */
	if (rc == SLURM_SUCCESS) {
		cpus = xmalloc(bit_set_count(node_map) * sizeof(uint16_t));
		start = 0;
		a = 0;
		for (n = 0; n < cr_node_cnt; n++) {
			if (bit_test(node_map, n)) {
				cpus[a++] = cpu_cnt[n];
				if (cr_get_coremap_offset(n) != start) {
					bit_nclear(core_map, start,
						   (cr_get_coremap_offset(n))-1);
				}
				start = cr_get_coremap_offset(n + 1);
			}
		}
		if (cr_get_coremap_offset(n) != start) {
			bit_nclear(core_map, start, cr_get_coremap_offset(n)-1);
		}
	}
    
    return NULL;
}


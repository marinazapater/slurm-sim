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
    int rc ;
    char *nodes;
    char str[1000];
    uint16_t *cpus = NULL;
    uint16_t *cpu_cnt;
    
    extalloc_print_job_bitmap(node_map);
    extalloc_print_job_bitmap(core_map);
    nodes = bitmap2node_name(node_map);
    info("external_allocator: non-free nodes %s", nodes);
    info("external_allocator: job comment %s", job_ptr->comment);
    info("external_allocator: requested cpus %d", job_ptr->details->cpus_per_task);
    sprintf(str, "./external_allocator.sh %s %s %d %d", nodes, job_ptr->comment,
            job_ptr->job_id, job_ptr->details->cpus_per_task);
    rc = system(str);

    //XXX-marina: need to retrieve allocation data and convert to bitmap!!
    /* if successful, sync up the core_map with the node_map, and */
	/* create a cpus array */
	if (rc == SLURM_SUCCESS) {
        FILE *fr;
        char filename[100];
        sprintf(filename, "/tmp/alloc_btm_%d.txt", job_ptr->job_id);
        fr = fopen (filename, "r");
        if (fr == NULL){
            info("extern_alloc: error opening bitmap file for reading\n");
            return NULL;
        }
        
        //Reading bitmap file line by line and filling structures
        char line[1000];
        int res=0, node_i=0, i=0, size=0;
        int core_start_bit, core_end_bit, cpu_alloc_size;
        char *token = NULL;
        char nodename[20];
        int num_cores_used = 0;

        info("Getting resource usage");
        _get_res_usage(job_ptr, node_map, core_map, cr_node_cnt,
                       node_usage, cr_type, &cpu_cnt, false);

        info("Generating bitmap");
        size=bit_size(node_map);
        bit_nclear(node_map, 0, size-1);
        size=bit_size(core_map);
        bit_nclear(core_map, 0, size-1);
        
        while (fgets(line,sizeof(line), fr) != NULL){
            bitstr_t *my_bitmap = NULL;

            // Reading nodename and number of used cores from bitmap
            info("extern alloc: line is %s", line);
            token = strtok( line, "," );
            if (token == NULL){
                info("extern_alloc: error parsing node name");
                fclose(fr);
                return NULL;
            }
            strcpy( nodename, token );
            token = strtok( NULL, ",\n");
            if (token == NULL){
                info("extern_alloc: error parsing cores used");
                fclose(fr);
                return NULL;
            }
            num_cores_used = atoi(token);
            info("extern_alloc: nodename: %s, cores used: %d",
                 nodename, num_cores_used);
            
            // Getting bitmap position of node by name
            if (node_name2bitmap(nodename, 0, &my_bitmap) != 0){
                info("extern_alloc: node_name2bitmap failed");
                fclose(fr);
                return NULL;
            }
            res = bit_set_count(my_bitmap);
            if (res != 1){
                info("extern_alloc: did not get bitmap of node properly");
                info("extern_alloc: %d bits set", res );
            }
            node_i=bit_ffs(my_bitmap);
            bit_free(my_bitmap);
            
            // Setting allocation in bitmap 
            bit_set(node_map, node_i);
            core_start_bit = cr_get_coremap_offset(node_i);
            core_end_bit   = cr_get_coremap_offset(node_i+1) - 1;
            if (num_cores_used > (core_end_bit + 1 - core_start_bit)){
                info("Cores to be allocated (%d) greater than slots (%d). Cannot allocate",
                     num_cores_used, (core_end_bit + 1 - core_start_bit));
            }
            for (i=0; i<num_cores_used; i++){
                bit_set(core_map, core_start_bit + i);
            }
        }
        fclose(fr);
               
        info("extern_alloc: printing final bit sets");
        extalloc_print_job_bitmap(node_map);
        extalloc_print_job_bitmap(core_map);
        info("extern_alloc: cr_node_cnt is %d",cr_node_cnt);
       
        // Setting cpus accordingly
        uint32_t start, n, a;
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
    info("external_allocator: external allocation finished successfully");
    return cpus;
}

/* Calls to print_slurm_alloc generates a file:
   /tmp/alloc_out_{jobid}.txt where allocation is printed
   (so that it can be rsynced to dcsim)
 */
void print_slurm_alloc (struct job_record *job_ptr, bitstr_t *node_map,
                        uint16_t *cpu_cnt, uint32_t cr_node_cnt)
{
    int i=0;
    char *nodes;
    char cpus[500];
    cpus[0]='\0';
    char str[1000];
    nodes = bitmap2node_name(node_map);
    info(" const_res_ext: printing slurm allocation to file for %d nodes", cr_node_cnt);
    for (i=0; i< cr_node_cnt; i++){
        if (cpu_cnt[i]>0){
            info("cpu_cnt for node %d is greater than 0: %d",i,cpu_cnt[i]);
            char num[10];
            num[0]='\0';
            sprintf(num, "%d", (int) cpu_cnt[i]);
            strcat(cpus, num);
            //I break because I am only prepared for 1 node
            break;
        }
    }
    info(" const_res_ext: nodes are %s and cpus %s", nodes, cpus);
    sprintf(str, "./slurm_alloc_gen.sh %d %s %s", job_ptr->job_id, nodes, cpus);
    system(str);
    xfree(nodes);
}

/* Calls dcsim to compute energy when a task begins (begin=1) or ends (begin=0) */
uint16_t call_dcsim (struct job_record *job_ptr, bitstr_t *node_map, int begin, time_t now)
{
    //We should somehow give the current time to the simulator...
    char *nodes;
    char str[1000];
    int rc;
    if ((node_map == NULL) && (begin == EXTALLOC_TASK_BEGIN)){
        printf("Error. Cannot call DCsim because I got no nodemap");
        return 1;
    }
    nodes = bitmap2node_name(node_map);
    info("external_allocator: non-free nodes %s", nodes);
    info("external_allocator: job comment %s", job_ptr->comment);
    if (begin == EXTALLOC_TASK_BEGIN){
        printf("New task begun. Calling simulator.\n");
        sprintf(str, "./dcsim_caller.sh %d %ld jobbegin", job_ptr->job_id, now);
    }
    else if (begin == EXTALLOC_TASK_END){
        printf("New task ended. Calling simulator.\n");
        sprintf(str, "./dcsim_caller.sh %d %ld jobend", job_ptr->job_id, job_ptr->end_time);
    }
    else {
        printf("Error, invalid task type\n");
        return 1;
    }
    rc = system(str);
    xfree(nodes);
    return rc;
}


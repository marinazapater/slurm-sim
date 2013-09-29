#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>

#include "sim_trace.h"


int main(int argc, char *argv[]){

		int trace_file;
		job_trace_t job_trace;

		trace_file = open("test.trace", O_RDONLY);
		if(trace_file < 0){
				printf("Error opening test.trace\n");
				return -1;
		}

		printf("JOBID  \tUSERNAME  \tPARTITION  \tACCOUNT  \tQOS     \tSUBMIT    \tDURATION  \tWCLIMIT  \tTASKS\n");
		printf("=====  \t========  \t=========  \t=======  \t======  \t========  \t========  \t======== \t=====\n");

		while(read(trace_file, &job_trace, sizeof(job_trace))){
				printf("%5d  \t%8s  \t%9s  \t%7s  \t%6s  \t%8d  \t%8d  \t%7d  \t%5d(%d,%d)", job_trace.job_id, job_trace.username, job_trace.partition, job_trace.account, job_trace.qosname, job_trace.submit, job_trace.duration, job_trace.wclimit, job_trace.tasks, job_trace.tasks_per_node, job_trace.cpus_per_task);
                if(strlen(job_trace.reservation) > 0)
                    printf(" RES=%s", job_trace.reservation);
                if(strlen(job_trace.dependency) > 0)
                    printf(" DEP=%s", job_trace.dependency);

                printf("\n");

		}

		return 0;
}

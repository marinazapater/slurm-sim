#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>

#include "sim_trace.h"

#define MAX_WALLCLOCK	7200
#define MIN_WALLCLOCK	60

#define MAX_USERS 1000

#define MAX_LINE 1000

/* this program creates a file with a set of job traces for slurm simulator */

static struct option long_options[] = {
    {"jobsfile",      1, 0, 'j'},
    {NULL,       0, 0, 0}
};


/* job_id, user, partition, account, class, submit_time, duration, wclimit, tasks, tasks_per_node, cpus_per_task */

int trace_file;
int job_counter = 1001;
int total_cpus = 0;
int total_jobs = 0;
int submit_time = 0;
int equal_times = 0, padding = 0;
int thistime = 0, oldtime = 0;

int cpus_per_task = 0;
int tasks_per_node = 0;

job_trace_t new_trace;

char *username[1000];
int total_users = 0;

char *qosname[1000];
int total_qos = 0;

char *jobfilename;
char *default_partition;
char *default_account;

int main(int argc, char *argv[]){

	int i;
	int written;
	int  userfile;
	int  qosfile;
	int char_pos;
	int endfile = 0;
	char *name;
    int option_index;
    int opt_char;
	FILE*  jobfile;
    char buffer[MAX_LINE];

        while((opt_char = getopt_long(argc, argv, "j",
                        long_options, &option_index)) != -1) {
            switch (opt_char) {

                case (int)'j':
                    jobfilename = strdup(optarg);
                    break;

                /*case (int)'s':*/
                    /*submit_time = atoi(optarg);*/
                    /*break;*/

                default:
                    fprintf(stderr, "getopt error, returned %c\n",
                            opt_char);
                    exit(0);
            }
        }

        /*if(total_cpus == 0){*/
            /*printf("Usage: %s --cpus=xx --jobs=xx --partition=xxxx --account=xxxx --cpus_per_task=xx --tasks_per_node=xx --submit_time=xx(unixtime)\n", argv[0]);*/
            /*return -1;*/
        /*}*/
		
		if(jobfilename == NULL){
            printf("Usage: %s --jobfilename=xx\n", argv[0]);
            return -1;
        }

        //USERS
		userfile = open("users.sim", O_RDONLY);

		if(userfile < 0){
			printf("users.sim file not found\n");
			return -1;
		}

		while(1){
		  	char_pos = 0;
			name = malloc(30);
			while(1){
				if(read(userfile, &name[char_pos], 1) <= 0){
					endfile = 1;
					break;
				};
				//printf("Reading char: %c\n", username[char_pos][total_users]);
				if(name[char_pos] == '\n'){
					name[char_pos] = '\0';
					break;
				}
				char_pos++;
			}
			if(endfile)
				break;
			username[total_users] = name;
			printf("Reading user: %s\n", username[total_users]);
			total_users++;
		}
        
        //TEST.TRACE
		if((trace_file = open("test.trace", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) < 0){
				printf("Error opening file test.trace\n");
				return -1;
		}

        submit_time = 1316242565;

        //JOBS FILE
        jobfile = fopen(jobfilename, "r");
		if(jobfile < 0){
			printf("Error: Could not open job file\n");
			return -1;
		}

        equal_times=0; padding=0;
        thistime=0; oldtime=0;
        //Main loop
        while( fgets (buffer, MAX_LINE, jobfile) != NULL ){
            
            if (buffer[0] != '#') {
                
                //job_id    
                new_trace.job_id = job_counter++;
                //job_name
                sprintf(new_trace.jobname, "%s", strtok (buffer, " \t\n"));
                //submit_time
                thistime = atoi(strtok (NULL, " \t\n"));
                if (thistime==oldtime){
                    equal_times++;
                    /*if (equal_times % 30 == 0){*/
                        /*padding+=40;*/
                    /*}*/
                    if (equal_times % 10 == 0){
                        padding+=2;
                    }
                }
                else{
                    equal_times=0;
                    padding=0;
                }
                new_trace.submit = submit_time + thistime + padding;
                oldtime=thistime ;
                //username
                sprintf(new_trace.username, "%s", strtok (NULL, " \t\n"));
                //qos
                sprintf(new_trace.qosname, "%s", strtok (NULL, " \t\n"));
                //duration
                new_trace.duration = atoi(strtok (NULL, " \t\n"));
                if(new_trace.duration < 0){
                    printf("Error reading job file did not work. Exiting...\n");
                    return -1;
                }
                //tasks
                new_trace.tasks = atoi(strtok (NULL, " \t\n"));
                //wclimit
                new_trace.wclimit = 1.3*new_trace.duration;

                //account
                sprintf(new_trace.account, "%s", strtok (NULL, " \t\n"));
                //partition
                sprintf(new_trace.partition, "%s", strtok (NULL, " \t\n"));
                
                //cpus_per_task
                new_trace.cpus_per_task =  atoi(strtok (NULL, " \t\n"));
                //tasks_per_node
                new_trace.tasks_per_node =  atoi(strtok (NULL, " \t\n"));

                written = write(trace_file, &new_trace, sizeof(job_trace_t));

                printf("JOB(%s): %d, %d, %d(%d,%d)\n", new_trace.username, job_counter - 1, new_trace.duration, new_trace.tasks, new_trace.cpus_per_task, new_trace.tasks_per_node);
                if(written != sizeof(new_trace)){
                        printf("Error writing to file: %d of %ld\n", written, sizeof(new_trace));
                        return -1;
                }
            }
		}
        
        fclose(jobfile);
    return 0;
}

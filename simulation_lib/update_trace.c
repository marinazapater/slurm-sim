#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <errno.h>

#include "sim_trace.h"

/* This command modifies a job trace file depending on options given by the user:
 *
 * -R implies linking jobs to reservations
 * -D implies linking a job to other by dependency
 *
*/


static struct option long_options[] = {
    {"reservation",      0, 0, 'R'},
    {"dependency",  0, 0, 'D'},
    {"rsv_name",  1, 0, 'n'},
    {"jobid",     1, 0, 'j'},
    {"ref_jobid",     1, 0, 'r'},
    {"account",     1, 0, 'a'},
    {NULL,       0, 0, 0}
};

char *rsv_name;
int reservation_opt = 0;
int dependency_opt = 0;
int jobid;
char *ref_jobid;
char *account;

int main(int argc, char *argv[]){

    int trace_file, new_file;
    job_trace_t job_trace;
    int option_index;
    int opt_char;


    while((opt_char = getopt_long(argc, argv, "RDnjra",
                    long_options, &option_index)) != -1) {
        switch (opt_char) {
            case (int)'R':
                printf("Reservation option\n");
                reservation_opt = 1;
                break;

            case (int)'D':
                printf("Dependency option\n");
                dependency_opt = 1;
                break;

            case (int)'n':
                rsv_name = strdup(optarg);
                printf("Parsing reservation name to %s\n", rsv_name);
                break;

            case (int)'j':
                jobid = atoi(optarg);
                printf("Parsing jobid to %d\n", jobid);
                break;

            case (int)'r':
                ref_jobid = strdup(optarg);
                break;

            case (int)'a':
                account = strdup(optarg);
                printf("Parsing account to %s\n", account);
                break;

            default:
                fprintf(stderr, "getopt error, returned %c\n",
                        opt_char);
                exit(0);
        }
    }

    if(!reservation_opt && !dependency_opt){
        printf("Command needs to specify reservation or dependency action\n");
        return -1;
    }

    if(reservation_opt){
        if((rsv_name == NULL) || (jobid == 0) || (account == NULL)){
            printf("Reservation option needs:\n\t --rsv_name and \n\t--jobid\n\t--account\n");
            return -1;
        }
    }
    if(dependency_opt){
        if((ref_jobid == NULL) || (jobid == 0)){
            printf("Dependency option needs --jobid and --ref_jobid\n");
            return -1;
        }
    }


    trace_file = open("test.trace", O_RDONLY);
    if(trace_file < 0){
        printf("Error opening test.trace\n");
        return -1;
    }

    new_file = open(".test.trace.new", O_CREAT | O_RDWR, S_IRWXU);
    if(new_file < 0){
        printf("Error creating temporal file at /tmp\n");
        return -1;
    }

    while(read(trace_file, &job_trace, sizeof(job_trace))){

        if(reservation_opt){
            if(job_trace.job_id != jobid){
                write(new_file, &job_trace, sizeof(job_trace));
                continue;
            }
           
            sprintf(job_trace.reservation, "%s", rsv_name);
            sprintf(job_trace.account, "%s", account);
        }

        if(dependency_opt){
            if(job_trace.job_id != jobid){
                write(new_file, &job_trace, sizeof(job_trace));
                continue;
            }

            sprintf(job_trace.dependency, "%s", ref_jobid);
        }

        write(new_file, &job_trace, sizeof(job_trace));
    }

    close(trace_file);
    close(new_file);

    if(rename(".test.trace.new", "./test.trace") < 0){
        printf("Error renaming file: %d\n", errno);
    };

    return 0;
}

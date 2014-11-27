/*******************************************************
 * SLURM simulator launcher code
 *******************************************************/

#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <linux/tcp.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <poll.h>
#include <signal.h>

#include<ctype.h>

#include <sim_trace.h>
#include <slurm_sim.h>

#include <sys/mman.h>
#include <semaphore.h>

#include <config.h>
#include <slurm/slurm_errno.h>

#include <src/common/forward.h>
#include <src/common/hostlist.h>
#include <src/common/node_select.h>
#include <src/common/parse_time.h>
#include <src/common/slurm_accounting_storage.h>
#include <src/common/slurm_jobcomp.h>
#include <src/common/slurm_protocol_pack.h>
#include <src/common/switch.h>
#include <src/common/xassert.h>
#include <src/common/xstring.h>
#include <src/common/assoc_mgr.h>

#include "comm_protocol.h"

//XXX-marina: this was set to 50 at the beginning
#define MAX_INDEPENDENT_THREADS 500

#define MONITOR_INTERVAL 300

#undef DEBUG
       
int sim_mgr_debug_level = 9;

#define sim_mgr_debug(debug_level, ...) \
    ({ \
     if(debug_level >= sim_mgr_debug_level) \
     printf(__VA_ARGS__); \
     })


#undef MONITOR

#define IS_ACTIVE_SLOT(i)   (sleep_map_array[0] & (1ULL << i))
#define HAS_THREAD_FINISHED(i) (thread_exit_array[0] & (1ULL << i))

#define RELEASE_SLOTS() \
        sleep_map_array[0] &= ~thread_exit_array[0]; \
        thread_new_array[0] &= ~thread_exit_array[0]; \
        *(thread_exit_array) =  0; 

int ffsll(long long int i);

/* simulation time starting point */
long int sim_start_point;
/* simulation end point is an optional parameter */
long int sim_end_point;

job_trace_t *trace_head, *trace_tail;
rsv_trace_t *rsv_trace_head, *rsv_trace_tail;

/* Pointer to shared memory */
void *timemgr_data;

/* There are some threads out of sim_mgr control like those doing rpc management */
/* In other case it would lead to a deadlock */
unsigned long *independent_threads[MAX_INDEPENDENT_THREADS];
char *independent_thread_name[MAX_INDEPENDENT_THREADS];
                        
/* global data */
sem_t *global_sem;      
sem_t *thread_sem[MAX_THREADS];
sem_t *thread_sem_back[MAX_THREADS];
unsigned int *current_sim;
long long int *sleep_map_array;
long long int *thread_exit_array;
unsigned int *fast_threads_counter;
unsigned int *pthread_create_counter;
unsigned int *pthread_exit_counter;
long long int *thread_new_array;
thread_data_t *threads_data;
unsigned int *proto_threads;

#ifdef MONITOR
static pthread_mutex_t monitor_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t simulation_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t  monitor_start = PTHREAD_COND_INITIALIZER;
static pthread_cond_t  monitor_stop = PTHREAD_COND_INITIALIZER;
#endif

/* path to sbatch  & scontrol programs */
char sbatch_bin[200];
char scontrol_bin[200];

char *global_envp[100];     /* Uhmmm ... I do not like this limitted number of env values */

/* global socket handling */
int udpsock;
struct pollfd my_fd;

void change_debug_level(int signum){

	if(sim_mgr_debug_level > 0)
		sim_mgr_debug_level = 0;
	else
		sim_mgr_debug_level = 9;
}

/* Debugging */
void dumping_shared_mem(int signum){

    int i;
    struct timeval t1;

    printf("Let's dump the shared memory contents\n");

    gettimeofday(&t1, NULL);
    printf("SIM_MGR[%u][%ld][%ld]: created,exited: %d,%d\n", current_sim[0], t1.tv_sec, t1.tv_usec, pthread_create_counter[0], pthread_exit_counter[0]);
    printf("sleep_map_array: %16llx\n", sleep_map_array[0]);
    printf("thread_exit_array: %16llx\n", thread_exit_array[0]);
    printf("Total fast threads: %u\n", fast_threads_counter[0]);
    printf("Total thread create counter: %u\n", pthread_create_counter[0]);
    printf("Total thread exit counter: %u\n", pthread_exit_counter[0]);

    printf("Dumping thread data ...\n\n");

    printf("Thread\t\tpid\t\tf_address\t\tsl/new/noend/join\tcreation\tlast_sleep\tlast_wakeup\twait_mean\n");
    printf("======\t\t===\t\t=========\t\t=================\t==========\t===========\t========\t========\n");

    for(i = 0; i < MAX_THREADS; i++){

        if(threads_data[i].wait_count > 0)
            printf("%d\t\t%d\t\t%016lx\t\t%d/%d/%d/%d\t\t\t%08ld\t%08ld\t%08ld\t%lld\n", i, threads_data[i].pid, 
                            threads_data[i].func_addr, threads_data[i].sleep, threads_data[i].is_new,
                            threads_data[i].never_ending, threads_data[i].joining, 
                            threads_data[i].creation, threads_data[i].last_sleep, 
                            threads_data[i].last_wakeup,
                            threads_data[i].wait_time / threads_data[i].wait_count);
        else
            printf("%d\t\t%d\t\t%016lx\t\t%d/%d/%d/%d\t\t\t%08ld\t%08ld\t%08ld\t%lld\n", i, threads_data[i].pid, 
                            threads_data[i].func_addr, threads_data[i].sleep, threads_data[i].is_new,
                            threads_data[i].never_ending, threads_data[i].joining, 
                            threads_data[i].creation, threads_data[i].last_sleep, threads_data[i].last_wakeup, 
                            0LL);
    }

#if 0
    printf("Dumping semaphores state:\n");
    for(i = 0; i < MAX_THREADS; i++){
        sem_getvalue(thread_sem[i], &sval);
        printf("[%d]: %d\n", i, sval);
        sem_getvalue(thread_sem_back[i], &sval);
        printf("[%d]BACK: %d\n", i, sval);
    }
#endif

    /* Let's kill slurmctld and slurmd */
    /* This is not the way you should stop slurm in a real environment but it helps for simulation debugging */
    kill(threads_data[0].pid, SIGSEGV);
    kill(threads_data[32].pid, SIGSEGV);

    if(signum == SIGINT)
        exit(0);

    return;
}

int is_independent_thread(unsigned long *addr){

    int i = 0;

    while(i < MAX_INDEPENDENT_THREADS){
            if(addr == independent_threads[i++])
                return 1;
        }

    return 0;
}

int wait_thread_running(int i){

    struct timeval t1;

    gettimeofday(&t1, NULL);

    while(1){ 
        
        struct timespec waiting;
        int waitloops = 0;
        struct timeval l_tv;

        if((threads_data[i].never_ending) || (threads_data[i].joining) || (threads_data[i].sleep >= 0))
            break;

        /* Next two clauses are redundant. Do we need them both? */
        if(HAS_THREAD_FINISHED(i))
            break;

        if(!IS_ACTIVE_SLOT(i))
            break;

        sem_wait(global_sem);

        gettimeofday(&l_tv, NULL); 
        sim_mgr_debug(0, "[%ld-%ld] Releasing thread slots from exit array\n", l_tv.tv_sec, l_tv.tv_usec);
        sim_mgr_debug(0, "[%016llx][%016llx]\n", sleep_map_array[0], thread_exit_array[0]);

        /* Let's release slots of finished threads */
        RELEASE_SLOTS();

        sim_mgr_debug(0, "[%016llx][%016llx]\n", sleep_map_array[0], thread_exit_array[0]);

        sem_post(global_sem);

        /* Let's leave some time to execute */
        waiting.tv_sec = 0;
        waiting.tv_nsec = 1000000;
        nanosleep(&waiting, 0);

        waitloops++;
        if(waitloops >= 30000){
            printf("ERROR: Too much wait loops in wait_thread_running\n");
            printf("sleep_map_array: %16llx\n", sleep_map_array[0]);
            printf("thread_exit_array: %16llx\n", thread_exit_array[0]);
            dumping_shared_mem(SIGSEGV);
            pthread_exit(0);
        }
    }

    return 0;

}


/* New slurm threads appear when:
  - a job is dispatched
  - a job completes 
  - a job is submitted
 
  Usually new threads will send messages then other new threads are created for processing those messages.
  This is the reason behind reserving half of simulation thread slots to slurmd and half to slurmctld because
  a new slutmctld thread will need a new slurmd thread to keep going so it is not good if one program can use all
  thread slots since during a cycle when a lot of jobs are submitted, dispatched or finished, deadlocks can appear due to 
  lack of free simulation thread slots. 
  As most of new threads execute fast, those slots need to be released under a burst of new threads.

  This needs to be called with global_sem closed 
 */
int checking_for_new_threads(){

    int new;

    while(1){
    
        struct timespec waiting;

        /* Checking for new threads created during this cycle and no checked yet */
        sim_mgr_debug(3, "Checking for new threads [%16llx] created during this cycle and no checked yet\n", thread_new_array[0]);

        while((new = ffsll(thread_new_array[0])) > 0)
        {
            new--;  /* ffs returns ordinal */

            /* Releasing threads slots */
            sim_mgr_debug(3, "SMA: %16llx, TEM: %16llx\n", sleep_map_array[0], thread_exit_array[0]);

            sleep_map_array[0] &= ~(thread_exit_array[0]);

            sim_mgr_debug(3, "SMA: %16llx, TEM: %16llx\n", sleep_map_array[0], thread_exit_array[0]);

            thread_new_array[0] &= ~(thread_exit_array[0]);

            if(HAS_THREAD_FINISHED(new)){
                /* a thread fast like Bolt */
                /* Let's release resources */
                thread_exit_array[0] =  0;
                continue;
            };

            thread_exit_array[0] =  0;

            sim_mgr_debug(3, "thread_new_array: %16llx\n", thread_new_array[0]);

            if(threads_data[new].is_new){

                /* This check is for rpc threads. Does it make sense for a new thread? */
                /* Probably this is just true during first simulation cycle */
                if(is_independent_thread((unsigned long *)threads_data[new].func_addr)){
                    printf("Thread[%d] is NEW and a RPC thread\n", new);
                    threads_data[new].never_ending = 1;
                }
                else{
                    sim_mgr_debug(3, "Thread[%d] is NEW. Waiting until that thread exits or sleeps\n", new);
                    sem_post(global_sem);
                    sim_mgr_debug(3, "CALLING wait_thread_running_new\n");
                    wait_thread_running(new);
                    sem_wait(global_sem);
                }
            }
            threads_data[new].is_new = 0;
            thread_new_array[0] &= ~(1ULL << (new));
        }

        if((proto_threads[0] == 0) && (thread_new_array[0] == 0))
            break;

        /* Lets give some time for those proto threads being scheduled */
        sem_post(global_sem);

        waiting.tv_sec = 0;
        waiting.tv_nsec = 10000000;
        nanosleep(&waiting, 0);

        sem_wait(global_sem);

        if((proto_threads[0] == 0) && (thread_new_array[0] == 0))
            break;

        /* We have more work to do */
        sim_mgr_debug(3, "PROTO_THREADS: %d\n", proto_threads[0]);
    }

}

//XXX-marina: SLURM-sim external allocator iface
int connect_ext_allocator(int port)
{
    //Socket data
    int val=1;
    struct sockaddr_in sin;

    //Server socket connection
    udpsock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    val=1;
    setsockopt(udpsock, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    if ( bind(udpsock, (struct sockaddr *) &sin, sizeof(sin)) < 0 ){
        printf("Error when binding udp socket!\n");
    }
    my_fd.fd = udpsock;
    my_fd.events = POLLIN;
    printf("SOCKET: Udp socket succesfully created on port %d\n", port);

    return 0;
}

/* This function checks if I need to halt slurm simulator */                    
int check_halt_slurmsim()
{
    int res, bytecnt;
    char buffer[500];
    
    my_fd.fd = udpsock;
    my_fd.events = POLLIN;

    //Checking if a I need to halt
    res = poll(&my_fd, 1, 10);
    if(res > 0) {
        printf("Poll returned a value\n");
        bytecnt = recv(udpsock, buffer, sizeof(buffer), 0);
        if(bytecnt > 0){
            printf("Received buffer is: %s\n", buffer);
            if (strstr(buffer, "halt") != NULL){
                printf("Halt received. Waiting until I receive more data\n");
                bytecnt=recv(udpsock, buffer, sizeof(buffer), 0);
                printf("Received buffer is: %s\n", buffer);
                printf("Continuing execution of slurm-sim\n");
            }
            else {
                printf("I didn't receive what I was expecting...\n");
            }
        }
        else {
            return -1;
        }
    }
    return 0;
}

/* This is the main simulator function */
static void *time_mgr(void *arg){

    unsigned int *current_micro;
    unsigned int *current_threads;
    int child;
    long int wait_time;
    struct timeval t1, t2;
    int i, j;
    int monitor_countdown;
    
    /* TODO: Submit should be more flexible. This is fine for Marenostrum usual job but it should cover all sbatch possibilities */
    //XXX-marina making this compatible at least with trace_build_from_file
    char *child_args[16] = { "", "--workdir=/tmp",
        "--error=/tmp/dumb", 
        "--output=/tmp/dumb",
        "", "", 
        "",
        "--no-requeue", 
        "",
        "",
        "",
        "",
        "",
        "",
        "test_command.cmd", (char *) 0 };

    printf("INFO: Creating time_mgr thread\n");

    current_sim = timemgr_data + SIM_SECONDS_OFFSET;
    current_micro = timemgr_data + SIM_MICROSECONDS_OFFSET;
    current_threads = timemgr_data + SIM_THREADS_COUNT_OFFSET;
    pthread_create_counter = timemgr_data + SIM_PTHREAD_CREATE_COUNTER;
    pthread_exit_counter = timemgr_data + SIM_PTHREAD_EXIT_COUNTER;
    proto_threads = timemgr_data + SIM_PROTO_THREADS_OFFSET;
    sleep_map_array = timemgr_data + SIM_SLEEP_ARRAY_MAP_OFFSET;
    thread_exit_array = timemgr_data + SIM_THREAD_EXIT_MAP_OFFSET;
    thread_new_array = timemgr_data + SIM_THREAD_NEW_MAP_OFFSET;
    threads_data = timemgr_data + SIM_SYNC_ARRAY_OFFSET;
    fast_threads_counter = timemgr_data + SIM_FAST_THREADS_OFFSET;

    current_sim[0] = sim_start_point;
    current_micro[0] = 0;
    current_threads[0] = 0;

#ifdef MONITOR
    printf("Waiting for sim_mon to connect...");

    pthread_mutex_lock(&simulation_lock);
    pthread_cond_wait(&monitor_start, &simulation_lock);
    pthread_mutex_unlock(&simulation_lock);

    printf("CONNECTED.\n");
#endif

    printf("Leaving some time for slurm threads to be ready ...\n");

    while(1){
        sleep(1);
        /* Borrowing thread_exit_array per initialization ... */
        if(thread_exit_array[0] == 2) /* Waiting for 2 processes to execute sim_init: slurmctld and slurmd */
            break;
    }

    /* Resetting thread_exit_array */
    thread_exit_array[0] = 0;

    sem_wait(global_sem);

    /* Now we can leave slurm daemons go */
    for(i = 0; i < MAX_THREADS; i++){

        if(sleep_map_array[0] & (1ULL << i)){
            printf("Sync: waking up thread %d\n", i);
            sem_post(thread_sem[i]);
        }
    }

    /* Giving time for main threads creation */
    sleep(1);

#ifdef MONITOR
    monitor_countdown = MONITOR_INTERVAL;
#endif

    gettimeofday(&t1, NULL);
       
    printf("SIM_MGR[%u][%ld][%ld]: Checking for %d threads [%016llx], last_cycle (created,exited): %d,%d\n", current_sim[0], t1.tv_sec, t1.tv_usec, current_threads[0], sleep_map_array[0], pthread_create_counter[0], pthread_exit_counter[0]);


    /* Main simulation manager loop */
    while(1){ 
        
        /* Do we have to end simulation in this cycle? */
        if(sim_end_point == current_sim[0]){
            printf("Ending simulation this cycle");
            dumping_shared_mem(SIGSEGV);
        }

#ifdef MONITOR
        if(monitor_countdown == 0){
            /* Monitor thread starts to work */
            pthread_mutex_lock(&monitor_lock);
            pthread_cond_signal(&monitor_start);
            pthread_cond_wait(&monitor_stop, &monitor_lock);
            pthread_mutex_unlock(&monitor_lock);

            monitor_countdown = MONITOR_INTERVAL;
        }
        
        monitor_countdown--;
#endif

        /* First going through threads and leaving a chance to execute code */

        gettimeofday(&t1, NULL);

        sim_mgr_debug(3, "SIM_MGR[%u][%ld][%ld]: Checking for %d threads [%016llx], last_cycle (created,exited): %d,%d\n",
                      current_sim[0], t1.tv_sec, t1.tv_usec, current_threads[0],
                      sleep_map_array[0], pthread_create_counter[0], pthread_exit_counter[0]);

        /* With global sem locked any thread can be created. Resetting create and exit counters */
        pthread_create_counter[0] = 0;
        pthread_exit_counter[0] = 0;

        for(i = 0; i < MAX_THREADS; i++){

            /* if this is Main slurmd thread or slot is not actived or this is a new thread*/
            if((i == 0) || (!IS_ACTIVE_SLOT(i)) || (threads_data[i].is_new))
                continue;

            /* Waiting for new threads to finish is good for determinsm */
            checking_for_new_threads();

            sim_mgr_debug(3, "Is thread %d running?\n", i);
            sem_post(global_sem);
            gettimeofday(&t1, NULL);
            wait_thread_running(i);
            sem_wait(global_sem);

            gettimeofday(&t2, NULL);
            wait_time = ((t2.tv_sec - t1.tv_sec) * 1000000) - t1.tv_usec + t2.tv_usec;
            sim_mgr_debug(3, "Waiting for: %ld\n", wait_time);
            sim_mgr_debug(3, "Checking active thread %d (%016llx)\n", i, sleep_map_array[0]);

            if(threads_data[i].sleep >= 0){

                sim_mgr_debug(3, "Thread[%d] waiting for %d seconds\n", i, threads_data[i].sleep);

                /* We handle new threads later on */
                if(threads_data[i].is_new)
                    continue;

                if(threads_data[i].sleep == 0){

                    int sval;
                    struct timespec waiting_back;
                    int res_sem;
                    struct timeval l_tv;

                    /* We have a thread to wake up */

                    sem_getvalue(thread_sem_back[i], &sval);
                    sim_mgr_debug(3, "Waking up thread %d (sem_back: %d)\n", i, sval);

                    sem_post(thread_sem[i]);
                    gettimeofday(&t1, NULL);

                    /* Always waiting until thread sleeps again or exits */
                    sem_post(global_sem);

                    gettimeofday(&l_tv, NULL);
                    res_sem = -1;

                    /* while waiting new threads could need a slot so we release slots of
                       threads exiting  for avoiding a potential deadlock */
                    while(res_sem < 0){

                        waiting_back.tv_sec = l_tv.tv_sec;
                        waiting_back.tv_nsec = (l_tv.tv_usec * 1000) + 1000000;

                        res_sem = sem_timedwait(thread_sem_back[i], &waiting_back);

                        sem_wait(global_sem);
                        gettimeofday(&l_tv, NULL);
                        sim_mgr_debug(3, "[%ld-%ld] Releasing thread slots from exit array\n", l_tv.tv_sec, l_tv.tv_usec);
                        sim_mgr_debug(3, "[%016llx][%016llx]\n", sleep_map_array[0], thread_exit_array[0]);

                        RELEASE_SLOTS();

                        sim_mgr_debug(3, "[%016llx][%016llx]\n", sleep_map_array[0], thread_exit_array[0]);
                        sem_post(global_sem);
                    }

                    /* At this point thread has gone to sleep again or maybe it has just exited */

                    gettimeofday(&t2, NULL);
                    sem_wait(global_sem);
                    wait_time = ((t2.tv_sec - t1.tv_sec) * 1000000) - t1.tv_usec + t2.tv_usec;

                    sim_mgr_debug(3, "Waiting for thread %d back...%ld\n", i, wait_time);
                    threads_data[i].wait_count++;
                    threads_data[i].wait_time += wait_time;
                }
                else{
                    sim_mgr_debug(3, "Timer for thread %i expires in %u\n", i, threads_data[i].sleep);
                }

            }
        }

        /* Main threads and threads created previously to this simulation cycle have got a chance to execute */
        /* Let's wait new threads to end before submiting new jobs */
        checking_for_new_threads();

        sem_post(global_sem);

        /* Now checking if a new reservation needs to be created */
        //printf("Checking reservations\n");
        if(rsv_trace_head && (current_sim[0] >= rsv_trace_head->creation_time)){

            int exec_result;
            printf("Creation reservation for %s [%d - %ld]\n", rsv_trace_head->rsv_command, current_sim[0], rsv_trace_head->creation_time);
            child = fork();

            if(child == 0){ /* the child */

                if(execve(rsv_trace_head->rsv_command, NULL, global_envp) < 0){
                    printf("Error in execve for %s\n", rsv_trace_head->rsv_command);
                    printf("Exiting...\n");
                    exit(-1);
                }
            }

            waitpid(child, &exec_result, 0);
            if(exec_result == 0)
                sim_mgr_debug(9, "reservation created\n");
            else
                sim_mgr_debug(9, "reservation failed");

            rsv_trace_head = rsv_trace_head->next;
        }

        /* Now checking if a new job needs to be submitted */
        //printf("Checking new job submissions\n");
        while(trace_head){

            int hour,min,sec;
            int exec_result;
            static int failed_submissions = 0;

            /* Uhmm... This is necessary if a large number of jobs are submitted at the same time but it seems
             * to have an impact on determinism */
            sem_wait(global_sem);
            checking_for_new_threads();
            sem_post(global_sem);

/* #ifdef DEBUG */
/*             printf("time_mgr: current %u and next trace %ld\n", *(current_sim), trace_head->submit); */
/* #endif */
            if(*(current_sim) >= trace_head->submit){
                sim_job_msg_t req;
                slurm_msg_t req_msg;
                slurm_msg_t resp_msg;
                slurm_addr_t remote_addr;
                job_trace_t *temp_ptr;

                /* First Sending a slurm message to slurmd  using a special SLURM message ID for simulation 
                 * purposes including jobid and job duration */
                slurm_msg_t_init(&req_msg);
                slurm_msg_t_init(&resp_msg);

                req.job_id  = trace_head->job_id - failed_submissions;
                req.duration = trace_head->duration;
                req_msg.msg_type = REQUEST_SIM_JOB;
                req_msg.data     = &req;

                /* TODO: avoid using hardcoded slurmd port number */
                slurm_set_addr(&remote_addr, 6818, "localhost");

                req_msg.address = remote_addr;

                if (slurm_send_recv_node_msg(&req_msg, &resp_msg, 500000) < 0){
                    printf("check_events_trace: error in slurm_send_recv_node_msg\n");
                }

                sim_mgr_debug(0, "check_events_trace: I got the SLURM_OK. Let's do the fork\n");

                /* First some parameters updates using job trace information */
                //printf("Updating child_args params");
                child_args[8] = malloc(100);
                memset(child_args[8], '\0', 100);
                sprintf(child_args[8], "--ntasks-per-node=%d", trace_head->tasks);

                child_args[4] = malloc(100);
                memset(child_args[4], '\0', 100);
                sprintf(child_args[4], "--account=%s", trace_head->account);

                child_args[5] = malloc(100);
                memset(child_args[5], '\0', 100);
                sprintf(child_args[5], "--partition=%s", trace_head->partition);

                child_args[6] = malloc(100);
                memset(child_args[6], '\0', 100);
                sprintf(child_args[6], "--cpus-per-task=%d", trace_head->cpus_per_task);

                child_args[9] = malloc(100);
                memset(child_args[9], '\0', 100);
                hour = trace_head->wclimit / 3600;
                min = (trace_head->wclimit % 3600) / 60;
                sec = (trace_head->wclimit % 3600) % 60;
                sprintf(child_args[9],"--time=%02d:%02d:%02d", hour,min,sec);

                child_args[11] = malloc(100);
                memset(child_args[11], '\0', 100);
                sprintf(child_args[11],"--qos=%s", trace_head->qosname);

                child_args[12] = malloc(100);
                memset(child_args[12], '\0', 100);

                if(strlen(trace_head->reservation)> 0)
                    sprintf(child_args[12],"--reservation=%s", trace_head->reservation);
                //else
                //    sprintf(child_args[12],"--comment=using_normal_resources");
                if (strlen(trace_head->comment) > 0)
                    sprintf(child_args[12],"--comment=%s", trace_head->comment);

                child_args[10] = malloc(100);
                memset(child_args[10], '\0', 100);
                //sprintf(child_args[10],"--uid=%s", trace_head->username);
                if (trace_head->exclusive)
                    sprintf(child_args[10],"--exclusive");
                
                if(strlen(trace_head->jobname) > 0){
                    child_args[13] = malloc(100);
                    memset(child_args[13], '\0', 100);
                    sprintf(child_args[13], "--job-name=%s", trace_head->jobname);
                    /*printf("Jobname is: %s \n", trace_head->jobname);*/
                }

                child = fork();

                if(child == 0){ /* the child */

                    if(execve(sbatch_bin, child_args, global_envp) < 0){
                        printf("Error in execve for sbatch\n");
                        printf("Exiting...\n");
                        exit(-1);
                    }
                }

                waitpid(child, &exec_result, 0);
                if(exec_result == 0)
                    sim_mgr_debug(3, "sbatch done for jobid %d\n", trace_head->job_id);
                else{
                    printf("sbatch failed for \"jobid\" %d\n", trace_head->job_id);
                    failed_submissions++;
                }

                /* Let's free trace record */
                temp_ptr = trace_head;
                trace_head = trace_head->next;
                free(temp_ptr);

            }else{
                /* job trace list is ordered in time so nothing else to do */
                break;
            }
        }

        sem_wait(global_sem);

        /* New jobs submitted create new threads */
        checking_for_new_threads();

        /* Now it is safe to decrement sleep field for sleeping threads */
        for(i = 0; i < MAX_THREADS; i++){

            if(threads_data[i].sleep > 0)
                threads_data[i].sleep--;
        }

        sim_mgr_debug(3, "SMA: %16llx, TEM: %16llx\n", *(sleep_map_array), *(thread_exit_array));

        /* Releasing those slots of exited threads */
        *(sleep_map_array) &= ~(*(thread_exit_array));
        *(thread_exit_array) = 0;

        sim_mgr_debug(3, "SMA: %16llx, TEM: %16llx\n", *(sleep_map_array), *(thread_exit_array));
        sim_mgr_debug(3, "Threads management: created %d, exited %d\n", pthread_create_counter[0], pthread_exit_counter[0]);

        //XXX-marina: interface external_allocator-SlurmSim-DCSim
        /* Check if I need to stop runnning */
        check_halt_slurmsim();
        
        /* And finally we increment simulation time */
        *(current_sim) = *(current_sim) + 1;
    }

    return 0;

}

int insert_trace_record(job_trace_t *new){

    if(trace_head == NULL){
        trace_head = new;
        trace_tail = new;
    }
    else{
        trace_tail->next = new;
        trace_tail = new;
    }
    return 0;
}

int insert_rsv_trace_record(rsv_trace_t *new){

    if(rsv_trace_head == NULL){
        rsv_trace_head = new;
        rsv_trace_tail = new;
    }
    else{
        rsv_trace_tail->next = new;
        rsv_trace_tail = new;
    }
    return 0;
}

int init_trace_info(void *ptr, int op){

    job_trace_t *new_trace_record;
    rsv_trace_t *new_rsv_trace_record;
    static int count = 0;

    if(op == 0){
        new_trace_record = calloc(1, sizeof(job_trace_t));
        if(new_trace_record == NULL){
            printf("init_trace_info: Error in calloc.\n");
            return -1;
        }

        *new_trace_record = *(job_trace_t *)ptr;

        if(count == 0){
            sim_start_point = new_trace_record->submit - 600;
            /*first_submit = new_trace_record->submit;*/
        }

        count++;

        /* Next is hardcoded for Marenostrum node type */
        //new_trace_record->tasks_per_node = 4;
        //new_trace_record->cpus_per_task = 1;

#if 0
        /* Lets make a busy queue from the beginning */
        if(count < 300)
            new_trace_record->submit = first_submit;
#endif

        /* With traces from slurm database timelimit comes in minutes */
        new_trace_record->wclimit *= 60;

        /* We do not want jobs being canceled during simulation */
        if(new_trace_record->duration > new_trace_record->wclimit)
            new_trace_record->duration = new_trace_record->wclimit;


        /* Cheating a bit for studying wclimit impact  */
        //new_trace_record->wclimit = new_trace_record->duration;

        insert_trace_record(new_trace_record);
    }

    if(op == 1){
        new_rsv_trace_record = calloc(1, sizeof(rsv_trace_t));
        if(new_rsv_trace_record == NULL){
            printf("init_trace_info: Error in calloc for new reservation\n");
            return -1;
        }

        *new_rsv_trace_record = *(rsv_trace_t *)ptr;
        new_rsv_trace_record->next = NULL;

        printf("Inserting new reservation trace record for time %ld\n", new_rsv_trace_record->creation_time);

        insert_rsv_trace_record(new_rsv_trace_record);
    }

    return 0;

}


int init_job_trace(){

    int trace_file;
    job_trace_t new_job;
    int total_trace_records = 0;

    trace_file = open("test.trace", O_RDONLY);
    if(trace_file < 0){
        printf("Error opening file test.trace\n");
        return -1;
    }

    /* job_id, submit_delta, duration, tasks, tasks_per_node, cpus_per_task */
    while(read(trace_file, &new_job, sizeof(new_job))){ 

	init_trace_info(&new_job, 0);
        total_trace_records++;
    }


    printf("Trace initializarion done. Total trace records: %d\n", total_trace_records);
    close(trace_file);

    return 0;
}

int init_rsv_trace(){

    int trace_file;
    rsv_trace_t new_rsv;
    int total_trace_records = 0;
    int count;
    char new_char;
    char buff[20];

    trace_file = open("rsv.trace", O_RDONLY);
    if(trace_file < 0){
        printf("Error opening file rsv.trace\n");
        return -1;
    }

    new_rsv.rsv_command = malloc(100);
    if(new_rsv.rsv_command < 0){
	    printf("Malloc problem with reservation creation\n");
	    return -1;
    }

    memset(buff, '\0', 20);
    memset(new_rsv.rsv_command, '\0', 100);

    while(1){

        count = 0;

        /* First reading the creation time value */
        while(read(trace_file, &new_char, sizeof(char))){

            if(new_char == '=')
                break;

            buff[count++] = new_char;
        }

        if(count == 0)
            break;

        new_rsv.creation_time = (long int)atoi(buff);

        count = 0;

        /* then reading the script name to execute */
        while(read(trace_file, &new_char, sizeof(char))){

            new_rsv.rsv_command[count++] = new_char;

            if(new_char == '\n'){ 
                new_rsv.rsv_command[--count] = '\0';
                break;
            }
        }

#if DEBUG
        printf("Reading filename %s for execution at %ld\n", new_rsv.rsv_command, new_rsv.creation_time);
#endif

        init_trace_info(&new_rsv, 1);
        total_trace_records++;
        if(total_trace_records > 10)
            break;
    }


    printf("Trace initializarion done. Total trace records for reservations: %d\n", total_trace_records);
    close(trace_file);

    return 0;
}

/* IPC semaphores created use as a prefix the name of user executing sim_mgr */
int init_semaphores(){

    char sem_name[100];
    int i;

    /* First removing old semaphores...*/
    sem_unlink(SIM_GLOBAL_SEM);

    printf("Initializing semaphores...\n");
    global_sem = sem_open(SIM_GLOBAL_SEM, O_CREAT, S_IRUSR | S_IWUSR, 1);
    if(global_sem == SEM_FAILED){
        printf("global_sem can not be created\n");
        return -1;
    }

    for(i = 0; i < MAX_THREADS; i++){

        memset(&sem_name, '\0', 100);
        sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_PREFIX, i);

        /* Removing this semaphore from IPC space */
        sem_unlink(sem_name);

        thread_sem[i] = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR, 0);
        if(thread_sem[i] == SEM_FAILED){
            printf("Error opening semaphore number %d\n", i);
            return -1;
        }

        memset(&sem_name, '\0', 100);
        sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_BACK_PREFIX, i);
        /* Removing this semaphore from IPC space */
        sem_unlink(sem_name);

        thread_sem_back[i] = sem_open(sem_name, O_CREAT, S_IRUSR | S_IWUSR, 1);
        if(thread_sem_back[i] == SEM_FAILED){
            printf("Error opening semaphore number %d\n", i);
            return -1;
        }
    }

    return 0;
}


int building_shared_memory(){

    int fd;

    /* A healthy check */
    if((sizeof(thread_data_t) * MAX_THREADS) > 8000){
        printf("Possible problem with shared memory size\n");
        return -1;
    }

    fd = shm_open(SLURM_SIM_SHM, O_CREAT | O_RDWR, S_IRUSR | S_IWUSR);
    if(fd < 0){
        printf("Error opening %s\n", SLURM_SIM_SHM);
        return -1;
    }

    ftruncate(fd, 8192);

    if(init_semaphores() < 0){
        printf("semaphores initialization failed\n");
        return 1;
    }

    timemgr_data = mmap(0, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(!timemgr_data){
        printf("mmaping %s file can not be done\n", SLURM_SIM_SHM);
        return -1;
    }

    memset(timemgr_data, 0, 8192);

    return 0;

}

/* I do not like this approach for thread identification but ...*/
int reading_rpc_threads_info(){

    int fd;
    char *addr1;
    char *name1;
    int i,j;
    int name_mark;

    fd = open("rpc_threads.info", O_RDONLY);
    if(fd < 0){
        return -1;
    }

    addr1 = malloc(30);
    name1 = malloc(100);

    i = j = 0;
    name_mark = 0;

    /* rpc_thread.info has address and thread name by line separated by an # sign */
    while(read(fd, &addr1[i], 1)){

        if(addr1[i] == '\n'){
            name1[i] = '\0';
            independent_thread_name[j] = name1;

            printf("Read address %p\n", independent_threads[j]);
            printf("Thread name: %s\n", independent_thread_name[j]);

            i = 0;
            j++;
            name_mark = 0;
            if(j == MAX_INDEPENDENT_THREADS)
                break;

            /* Getting memory for another thread name */
            name1 = malloc(100);
        }
        else 
            if(addr1[i] == '#'){
                name_mark = 1;
                addr1[i] = '\0';
                independent_threads[j] = (unsigned long *)strtol(addr1, 0, 16);
                i = 0;
            }
            else {
                if((name_mark) && !(isspace(addr1[i]))){
                    name1[i] = addr1[i];
                    i++;
                }

                if((name_mark == 0) && !(isspace(addr1[i])))
                    i++;
            }
    }

    return 0;
}

#ifdef MONITOR

#define DEBUG_SERVER_PORT 23720

static void *debug_server(void *arg){

    int server_sock_fd, server_sock;
    int val;
    socklen_t len;
    struct sockaddr_in sin;
    struct sockaddr_in remote_addr;
    socklen_t remote_addrlen = sizeof(remote_addr);
    int req_type;
    int debug_log;
    char buffer_log[1024];

    debug_log = open("sim_debug_server.log", O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    if(debug_log < 0){
        printf("(debug_server): debug log file sim_debug_server.log problem. Exiting\n");
        pthread_exit(0);
    }

    server_sock_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(server_sock_fd < 0){
        memset(&buffer_log, 1024, '\0');
        sprintf(&buffer_log[0], "(debug_server): Something was wrong creating socket\n");
        write(debug_log, &buffer_log, strlen(buffer_log));
        pthread_exit(0);
    }

    memset(&buffer_log[0], 1024, '\0');
    sprintf(&buffer_log[0], "(debug_server): Opened log file...\n");
    write(debug_log, &buffer_log, strlen(buffer_log));

    val = 1;
    setsockopt(server_sock_fd, SOL_SOCKET, SO_REUSEADDR, &val, sizeof(int));


    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(INADDR_ANY);
    sin.sin_port = htons(DEBUG_SERVER_PORT);

    if(bind(server_sock_fd, (struct sockaddr *) &sin, sizeof(sin)) < 0){
        memset(&buffer_log, 1024, '\0');
        sprintf(&buffer_log[0], "(debug_server): problem binding socket\n");
        write(debug_log, &buffer_log, strlen(buffer_log));
        pthread_exit(0);
    }

    if(listen(server_sock_fd, 10) < 0){
        memset(&buffer_log, 1024, '\0');
        sprintf(&buffer_log[0], "(debug_server): problem listening to socket\n");
        write(debug_log, &buffer_log, strlen(buffer_log));
        pthread_exit(0);
    }

    if((server_sock = accept(server_sock_fd, (struct sockaddr *)&remote_addr, &remote_addrlen)) < 0){
        memset(&buffer_log, 1024, '\0');
        sprintf((char *)&buffer_log, "(monitor_server): accept error\n");
        write(debug_log, (char *)&buffer_log, strlen(buffer_log));
    }

    pthread_mutex_lock(&monitor_lock);

    /* We got the connection with external monitoring program*/
    pthread_cond_signal(&monitor_start);

    threads_data = timemgr_data + SIM_SYNC_ARRAY_OFFSET;

    while(1){

        unsigned int simtime;
        struct timeval t1;

        sim_mgr_debug(5, "DEBUG_SERVER: new loop\n");

        pthread_cond_wait(&monitor_start, &monitor_lock);

        sim_mgr_debug(5, "DEBUG_SERVER: Sending current simulation time %u\n", *current_sim);

        simtime = *current_sim;

        if(send(server_sock, &simtime, sizeof(unsigned int), 0) < sizeof(unsigned int)){
            memset(&buffer_log, 1024, '\0');
            printf("DEBUG_SERVER: send_error\n");
            sprintf((char *)&buffer_log, "(monitor_server): send error\n");
            write(debug_log, (char *)&buffer_log, strlen(buffer_log));
        }else{
            sim_mgr_debug(5, "DEBUG_SERVER: %d bytes sent\n", sizeof(unsigned int));
        };

        gettimeofday(&t1, NULL);
        if(send(server_sock, &t1.tv_sec, sizeof(unsigned int), 0) < sizeof(unsigned int)){
            memset(&buffer_log, 1024, '\0');
            printf("DEBUG_SERVER: send_error\n");
            sprintf((char *)&buffer_log, "(monitor_server): send error\n");
            write(debug_log, (char *)&buffer_log, strlen(buffer_log));
        }else{
            sim_mgr_debug(5, "DEBUG_SERVER: %d bytes sent\n", sizeof(unsigned int));
        };

        if(send(server_sock, &t1.tv_usec, sizeof(unsigned int), 0) < sizeof(unsigned int)){
            memset(&buffer_log, 1024, '\0');
            printf("DEBUG_SERVER: send_error\n");
            sprintf((char *)&buffer_log, "(monitor_server): send error\n");
            write(debug_log, (char *)&buffer_log, strlen(buffer_log));
        }else{
            sim_mgr_debug(5, "DEBUG_SERVER: %d bytes sent\n", sizeof(unsigned int));
        };

        pthread_cond_signal(&monitor_stop);

        while(recv(server_sock, &req_type, sizeof(int), 0) < 0){
            memset(&buffer_log, 1024, '\0');
            sprintf(&buffer_log[0], "(debug_server): recv error\n");
            write(debug_log, &buffer_log, strlen(buffer_log));
            continue;
        }

    }

    close(server_sock_fd);

}

#endif


int main(int argc, char *argv[], char *envp[]){

    pthread_attr_t attr;
    pthread_t id_server, id_mgr;   
    int i, port; 
    
    //Parse cmdline args
    if(argc != 4){
        printf("Usage %s sim_end_point server port\n", argv[0]);
        return -1;
    }
    sim_end_point = atoi(argv[1]);
    port = atoi(argv[3]);
    
    printf("MAIN: sim_mgr running\n");
        
    connect_ext_allocator(port);

    // Env vars
    i = 0;
    while(envp[i]){
        global_envp[i] = envp[i++];
    }

    /* We need to execute sbatch wherever the program was installed */
    if(getenv("SLURM_PROGRAMS") == NULL){
        printf("SLURM_PROGRAMS variable not set. It should point to sbatch program directory. Exiting\n");
        return -1;
    }

    sprintf((char *)&sbatch_bin, "%s/sim_sbatch", getenv("SLURM_PROGRAMS"));
    printf("Found sbatch program at %s\n", sbatch_bin);

    sprintf((char *)&scontrol_bin, "%s/scontrol", getenv("SLURM_PROGRAMS"));
    printf("Found scontrol program at %s\n", sbatch_bin);

    /* Using a generated file during installation for threads identification ... sucks but something like 
     * * using thread start address for function name resolution is not trivial */
    /* What would be needed is some way for linking slurm daemons binaries with rpc_threads.info data 
     * * ensuring they both are from same slurm version. A key shared by sim_mgr and rpc_threads.info? */
    if(reading_rpc_threads_info() < 0){
        printf("Error reading RPC threads info. Did you install rpc_threads.info correctly?\n");
        return -1;
    }

    if(building_shared_memory() < 0){
        printf("Error building shared memory and mmaping it\n");
        return -1;
    };

    if(init_job_trace() < 0){
        printf("An error was detected when reading trace file. Exiting...\n");
        return -1;
    }

    if(init_rsv_trace() < 0){
        printf("An error was detected when reading trace file. Exiting...\n");
        return -1;
    }

    pthread_attr_init(&attr);
    signal(SIGINT, dumping_shared_mem);
    signal(SIGHUP, dumping_shared_mem);
    signal(SIGUSR1, change_debug_level);

#ifdef MONITOR 
    while(pthread_create(&id_server, &attr, &debug_server, 0)) {
        printf("Debug server can not be executed. Exiting...\n");
        return -1;
    }
#endif

    pthread_attr_init(&attr);

    /* This is a thread for flexibility */
    while (pthread_create(&id_mgr, &attr, &time_mgr, 0)) {
        printf("Error with pthread_create for time_mgr\n"); 
        return -1;
    }

    pthread_join(id_mgr, NULL);
    pthread_join(id_server, NULL);
    sleep(1);

    return 0;
}


/*
########################################################################
#  THIS FILE IS AUTOMATICALLY CREATED BY slurm_sim.pl when             #
#	slurm configure is executed  	          		       #
#  								       #
#    IPC names are based on username compiling Slurm		       #
#    sim_mgr should be executed with same uid			       #
#    								       #
########################################################################
*/

#include <pthread.h>

//XXX-marina: careful! If you're to increase max_threads, beware that
// some off the definitions below will need to be changed too
// (however, I dunno how)
// If you try to change MAX_THREADS only, then the simulator won't work
#define MAX_THREADS 64

#define SLURM_SIM_SHM "/tester_slurm_sim.shm"

#define SIM_GLOBAL_SEM "tester_slurm_simulator"
#define SIM_LOCAL_SEM_PREFIX "tester_slurm_simulator_thread_sem_"
#define SIM_LOCAL_SEM_BACK_PREFIX "tester_slurm_simulator_thread_sem_back_"


/* Offsets */
#define SIM_SECONDS_OFFSET      0
#define SIM_MICROSECONDS_OFFSET 4

/* This set a MAX_THREAD upper limit to 64 */
#define SIM_SLEEP_ARRAY_MAP_OFFSET    8
#define SIM_THREAD_EXIT_MAP_OFFSET   16 
#define SIM_THREAD_NEW_MAP_OFFSET    24 
#define SIM_THREADS_COUNT_OFFSET	 32
#define SIM_PROTO_THREADS_OFFSET	 36
#define SIM_FAST_THREADS_OFFSET	 	40
#define SIM_PTHREAD_CREATE_COUNTER	 	44
#define SIM_PTHREAD_EXIT_COUNTER	 	48
#define SIM_PTHREAD_SLURMCTL_PID		52
#define SIM_PTHREAD_SLURMD_PID			56

#define SIM_SYNC_ARRAY_OFFSET        60

typedef struct thread_data {
		pthread_t ptid;
		pid_t pid;
		unsigned long func_addr;
		time_t creation;
		time_t last_sleep;
		time_t last_wakeup;
		time_t deletion;
		int sleep;
		int is_new;
		int never_ending;
		int joining;
		long long wait_time;
		long int wait_count;
} thread_data_t;

/* Each thread needs a sleep count value and a sem_t variable */
#define THREAD_DATA_SIZE    (sizeof(thread_data_t))


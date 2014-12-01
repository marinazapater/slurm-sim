#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#define _GNU_SOURCE
#include <dlfcn.h>
#include <sched.h>
#include <semaphore.h>

#include <pthread.h>
#include <string.h>
#include <pwd.h>

#include "slurm_sim.h"
#include "comm_protocol.h"

#include <sys/time.h>
#include <errno.h>

#include <execinfo.h>

//#define LIBC_PATH  "/lib64/libc.so.6"
#define LIBC_PATH  "/lib/i386-linux-gnu/libc.so.6"
#define LIBPTHREAD_PATH "/lib/i386-linux-gnu/libpthread.so.0"
//#define LIBC_PATH  "/lib/x86_64-linux-gnu/libc.so.6"
//#define LIBPTHREAD_PATH "/lib/x86_64-linux-gnu/libpthread.so.0"

#undef DEBUG
//#define DEBUG

int sim_lib_debug_level = 9;

#define sim_lib_printf(debug_level, ...) \
    ({ \
     if(debug_level >= sim_lib_debug_level) \
     printf(__VA_ARGS__); \
     })


int ffsll(long long int i);

int (*real_gettimeofday)(struct timeval *,struct timezone *) = NULL;
unsigned int (*real_sleep)(unsigned int) = NULL;
time_t (*real_time)(time_t *) = NULL;
int (*real_pthread_create)(pthread_t *, const pthread_attr_t *, void *(*start) (void *), void *);
void (*real_pthread_exit)(void *retval);
int (*real_pthread_join)(pthread_t thread, void **retval);

int get_new_thread_id();
int set_exit_map_array(int pos);
int set_new_thread(int pos);
int get_thread_id();

/* Pointer to shared memory initialized by sim_mgr */
void *timemgr_data;

sem_t *global_sem;      
sem_t *thread_sem[MAX_THREADS];
sem_t *thread_sem_back[MAX_THREADS];

/* Pointers to shared memory data */
unsigned int *current_sim;
unsigned int *current_micro;
thread_data_t *threads_data;
unsigned int *current_threads;
unsigned int *proto_threads;
long long int *sleep_map_array;
long long int *thread_exit_array;
long long int *thread_new_array;
unsigned int *fast_threads_counter;
unsigned int *pthread_create_counter;
unsigned int *pthread_exit_counter;
int *slurmctl_pid;
int *slurmd_pid;

/* Local data by program */
pthread_t main_thread;
int main_thread_id;

typedef struct thread_sem_info{
    int id;
    pthread_t p;
}thread_sem_info_t;

typedef struct sim_user_info{
    uid_t sim_uid;
    char *sim_name;
    struct sim_user_info_t *next;
}sim_user_info_t;

sim_user_info_t *sim_users_list;

/* This array links simulation threads id with pthreads id */
/* Now when a thread enters a wrapper function this array is used for simulation operations */
thread_sem_info_t thread_info_list[MAX_THREADS];

void pthread_exit(void *retval) {

    int tid = -1;
    int reps;

    sim_lib_printf(0, "Wrapper for pthread_exit [%lu]\n", pthread_self());
    if(pthread_self() == main_thread){

        /* this could happen if slurmd pthread_create fails several times and _service_connection
         * * is executed without using a thread */
        sim_lib_printf(0, "pthread_exit for main thread do nothing. Returning\n");

    }else{ 
        tid = get_thread_id();

        reps = 0;
        /* A thread could be created and executed so fast pthread_create wrapper could not set sync tables properly*/
        while((tid < 0) || (tid > 63)){

            /* Let's track how often this happens and leave some time for pthread_create wrapper to work */
            fast_threads_counter[0]++;
            usleep(10000);

            tid = get_thread_id();
            reps++;

            /* Something is wrong. Let's dump some information, got the global sem then stalls */
            if(reps == 1000){
                int i = 0;
                printf("Thread %ld: Too much calls to get_thread_id. Listing current threads...\n", pthread_self());
                sem_wait(global_sem);
                while(i < MAX_THREADS){
                    if(thread_info_list[i].p != 0)
                        printf("Thread %ld[%d]\n", thread_info_list[i].p, i);
                    i++;
                }
                while(i > 0);
            }
        }

        sim_lib_printf(0, "pthread_exit getting global_sem\n");
        /* We need the lock for updating global structures */
        sem_wait(global_sem);

        pthread_exit_counter[0]++;

        sim_lib_printf(0, "pthread_exit waking_up sim_mgr\n");

        /* Most of threads do not need this but it does not hurt */
        sem_post(thread_sem_back[tid]);

        sim_lib_printf(0, "pthread_exit closing semaphore\n");

        set_exit_map_array(tid);
        threads_data[tid].ptid = 0;

        /* Now simulation stuctures are updated. Real pthread_exit call can be done freely */
        sem_post(global_sem);

        sim_lib_printf(0, "pthread_exit for %d: calling real_pthread_exit\n", tid);

        real_pthread_exit(retval);
    }
}


int pthread_join(pthread_t thread, void **retval){

    int tid = -1;
    int ret;

    while(tid < 0)
        tid = get_thread_id();

    sim_lib_printf(0, "pthread_join wrapper for %d thread\n", tid);

    threads_data[tid].joining = 1;

    /* Calling real pthread_join */
    ret = real_pthread_join(thread, retval);

    threads_data[tid].joining = 0;

    return ret;
}

int pthread_create(pthread_t *thread, const pthread_attr_t *attr, void *(*start) (void *), void *arg){

    int ret;
    char sem_name[100];
    int s_id;
    int sval;
    int err = 0;
    int proto = 0;


    /* This wrapper executes the full code with global lock, even call to real pthread_create */
    sem_wait(global_sem);

    pthread_create_counter[0]++;

    /* First of all, we need a simulation thread id. Under thread creation bursts could it be no
     * * free slot is available. This is what we call a proto-thread. It should be enough to wait 
     * * some time till one slot is available. If the wait takes more than 10 seconds, something is
     * * wrong about the simulation */
    while(1){
        struct timespec waiting;

        s_id = get_new_thread_id();
        if(s_id < 0){
            if(proto == 0){
                proto = 1;
                proto_threads[0]++;
                printf("WAITING FOR AN SLOT proto_threads: %d\n", proto_threads[0]);
            }
            sem_post(global_sem);
        }
        else
            break;
        err++;
        pthread_yield();
        //XXX-marina: I changes the err=1000 by a higher number, as I have very long
        //bursts
        if(err == 1000){
            printf("WARNING: pthread_create wrapper returning EAGAIN!!!\n");
            proto_threads[0]--;
            return -EAGAIN;
        }
        waiting.tv_sec = 0;
        waiting.tv_nsec = 10000000;
        nanosleep(&waiting, 0);
        sem_wait(global_sem);
    }

    /* Once we got here the simulation thread is not a proto-thread any more */
    if(proto)
        proto_threads[0]--;

    sim_lib_printf(0, "I GOT AN SLOT: proto_threads: %d\n", proto_threads[0]);

    /* simulation thread initialization */
    /* Most of it is just for debugging except sleep field */
    threads_data[s_id].sleep = -1;
    threads_data[s_id].creation = *current_sim;
    threads_data[s_id].deletion = 0;
    threads_data[s_id].last_sleep = 0;
    threads_data[s_id].never_ending = 0;
    threads_data[s_id].last_wakeup = 0;
    threads_data[s_id].wait_time = 0;
    threads_data[s_id].wait_count = 0;
    threads_data[s_id].func_addr = (unsigned long)start;
    threads_data[s_id].pid = getpid();

    set_new_thread(s_id);

    /* Calling real pthread_create is needed at this point. We need pthread_id for linking 
     * * a simulation thread id with a pthread_id. It could happen a fast thread trying to use
     * * this linking table before an entry has been created for it, but we are aware of this.
     * * The link is done as one of the last steps because we need to work for leaving all
     * * stable before the thread starts working with simulation structures.
     * */
    ret = real_pthread_create(thread, attr, start, arg);

    /* Opening semaphores using simulation thread id */
    memset(&sem_name, '\0', 100);
    sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_PREFIX, s_id);

    sim_lib_printf(0, "Opening semaphore %s\n", sem_name);
    thread_sem[s_id] = sem_open(sem_name, 0);
    if(thread_sem[s_id] == SEM_FAILED){
        printf("Error opening semaphore number %d\n", s_id);
        return -1;
    }

    sem_getvalue(thread_sem[s_id], &sval);

    sim_lib_printf(0, "pthread_create: sval for %s = %d\n", sem_name, sval);

    memset(&sem_name, '\0', 100);
    sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_BACK_PREFIX, s_id);

    sim_lib_printf(0, "Opening semaphore %s\n", sem_name);

    thread_sem_back[s_id] = sem_open(sem_name, 0);
    if(thread_sem_back[s_id] == SEM_FAILED){
        printf("Error opening semaphore number %d\n", s_id);
        return -1;
    }

    sem_getvalue(thread_sem_back[s_id], &sval);

    sim_lib_printf(0, "pthread_create: sval for %s = %d\n", sem_name, sval);

    /* Sometimes previous thread using this semaphore left the wrong value */
    while(sval > 1){
        sem_wait(thread_sem_back[s_id]);
        sem_getvalue(thread_sem_back[s_id], &sval);
    }

    if(sval == 0)
        sem_post(thread_sem_back[s_id]);

    sim_lib_printf(0, "pthread_create: sval for %s = %d\n", sem_name, sval);

    /* Is this really needed??? */
    thread_info_list[s_id].id = s_id;

    sim_lib_printf(0, "Dentro de wrapper para pthread_create, getting sleep id %d [%lu]\n", thread_info_list[s_id].id , *(thread));

    /* This is the important step. After this a thread can work with simulation wrappers */
    thread_info_list[s_id].p = *thread;

    threads_data[s_id].is_new = 1;
    threads_data[s_id].ptid = *thread;

    sem_post(global_sem);

    return ret;
}


/* Sleep wrapper is self-suficient. It never calls real sleep function */
unsigned int sleep(unsigned int seconds){

    int tid = -1;
    int sval;

    sim_lib_printf(0, "SIM: sleep_wrapper calling sleep for %d secs (self: %ld - main: %ld)\n", seconds, pthread_self(), main_thread);

    if(seconds == 0) /* Just in case */
        return 0;

    /* There is a issue with synchronization between sim_mgr and a thread calling sleep. What is needed to be done is
       to update sleep field for this thread and wait till such amount of time has been consumed in simulation time using
       a explicit semaphore for this thread. It could happen sim_mgr handling this thread between updating sleep field and 
       going to sleep with the semaphore. You can not use global_sem for this two steps, of course, maybe using more sempahores
       could it be possible. However, this is possible to control */

    if(pthread_self() == main_thread){

        threads_data[main_thread_id].sleep = seconds;

        sem_getvalue(thread_sem_back[main_thread_id], &sval);

        if(sval > 0){
            sem_wait(thread_sem_back[main_thread_id]);
        }else{
            sim_lib_printf(0, "[%u]Waking up sim_mgr from %d\n", *(current_sim), main_thread_id);
            sem_post(thread_sem_back[main_thread_id]);
        }

        sim_lib_printf(0, "SIM: sleep_wrapper calling sleep for main thread: %d secs\n", seconds);
        sem_wait(thread_sem[main_thread_id]);

    }else{
        while(tid < 0)
            tid = get_thread_id();

        threads_data[tid].sleep = seconds;

        sem_getvalue(thread_sem_back[tid], &sval);
        if(sval > 0){
            sem_wait(thread_sem_back[tid]);
        }else{
            sim_lib_printf(0, "[%u]Waking up sim_mgr from %d\n", *(current_sim), tid);
            sem_post(thread_sem_back[tid]);
        }


        sim_lib_printf(0, "SIM: sleep_wrapper (sval: %d) calling sleep for thread %d [%lu]: %d secs\n", sval, tid, pthread_self(), seconds);
        threads_data[tid].last_sleep = *current_sim;
        sem_wait(thread_sem[tid]);
        threads_data[tid].last_wakeup = *current_sim;
    }

    sim_lib_printf(0, "SIM: sleep_wrapper exiting for %ld\n", pthread_self());

    return 0;
}


time_t time(time_t *t){

    sim_lib_printf(0, "time_wrapper: %u\n", *(current_sim));

    if(t)
        *t = *(current_sim);

    return *(current_sim);
};


int gettimeofday(struct timeval *tv, struct timezone *tz){


    tv->tv_sec = *(current_sim);

    if((slurmctl_pid[0] == getpid()) || (slurmd_pid[0] == getpid()))
        sem_wait(global_sem);

    *(current_micro) = *(current_micro) + 100;

    if((slurmctl_pid[0] == getpid()) || (slurmd_pid[0] == getpid()))
        sem_post(global_sem);

    tv->tv_usec = *(current_micro);


    sim_lib_printf(0, "SIM: gettimeofday_wrapper GOT the time data back: %ld - %ld\n", tv->tv_sec, tv->tv_usec);

    return 0;
}

uid_t sim_getuid(char *name){

    sim_user_info_t *aux;

    aux = sim_users_list;

#ifdef DEBUG
    printf("sim_getuid: starting for username %s\n", name);
#endif

    while(aux){
        if(strcmp(aux->sim_name, name) == 0){
#ifdef DEBUG
            printf("sim_getuid: found uid %u for username %s\n", aux->sim_uid, aux->sim_name);
#endif
            return aux->sim_uid;
        }
        aux = aux->next;
    }

    return -1;
}

char *sim_getname(uid_t uid){

    sim_user_info_t *aux;
    char *user_name;

    aux = sim_users_list;

    while(aux){
        if(aux->sim_uid == uid){

            user_name = malloc(100);
            memset(user_name,'\0',100);
            user_name = strdup(aux->sim_name);
            return user_name;
        }
        aux = aux->next;
    }

    return NULL;
}

int getpwnam_r(const char *name, struct passwd *pwd, 
               char *buf, size_t buflen, struct passwd **result){


    pwd->pw_uid = sim_getuid(name);
    if(pwd->pw_uid == -1){
        *result = NULL;
        printf("No user found for name %s\n", name);
        return ENOENT;
    }
    pwd->pw_name = strdup(name);
    printf("Found uid %u for name %s\n", pwd->pw_uid, pwd->pw_name);

    *result = pwd;

    return 0;
}

int getpwuid_r(uid_t uid, struct passwd *pwd,
               char *buf, size_t buflen, struct passwd **result){

    pwd->pw_name = sim_getname(uid);

    if(pwd->pw_name == NULL){
        *result = NULL;
        printf("No user found for uid %u\n", uid);
        return ENOENT;
    }
    pwd->pw_uid = uid;
    pwd->pw_gid = 100;  /* users group. Is this portable? */
    printf("Found name %s for uid %u\n", pwd->pw_name, pwd->pw_uid);

    *result = pwd;

    return 0;

}


int set_new_thread(int pos){

    sim_lib_printf(0, "set_new_thread for thread_id: %d\n", pos);
    thread_new_array[0] |= (1ULL << pos);

    return 0;
}

int set_exit_map_array(int pos){

    sim_lib_printf(0, "set_exit_map_array for thread_id: %d, pthread: %ld, thread_exit_array: %16lld\n", pos, pthread_self(), thread_exit_array[0]);

    thread_exit_array[0] |= (1ULL << pos);
    thread_info_list[pos].p = 0;
    current_threads[0]--;

    return 0;
}


int get_thread_id(){

    int i = 0;
    pthread_t this;

    this = pthread_self();

    sim_lib_printf(0, "get_thread_id: %ld (main: %ld)\n", this, main_thread);
    if(this == main_thread){
        return 0;
    }

    while(i < MAX_THREADS){

        if(thread_info_list[i].p == this){
            return thread_info_list[i].id;
        }

        i++;
    }

    /* During thread creation could be a problem of thread running but thread_info_list not created yet */
    /* Returning -1 means try again later */

    return -1;
}


/* This should be called with global_sem protection */
int get_new_thread_id(pthread_t *thread){

    long long int map;
    struct timeval l_tv;

    map = sleep_map_array[0];

    /* First 32 thread slots are for slurmd, last 32 ones for slurmctld */
    if(slurmd_pid[0] == getpid())
        map |= 0xFFFFFFFF00000000ULL;
    else
        map |= 0xFFFFFFFFULL;

    map = ~map;

#if 0
    real_gettimeofday(&l_tv, NULL);
    sim_lib_printf(0, "[%ld-%ld] Using map: %016llx\n", l_tv.tv_sec, l_tv.tv_usec, map);
    sim_lib_printf(0, "get_new_thread_id: [%16llx][%016llx], threads counter= %d\n", sleep_map_array[0], thread_exit_array[0], current_threads[0]);
#endif

    /* Getting first slot available */
    map = ffsll(map);

    if(map == 0){
        /*printf("WARNING!: space no available for a new threads. Current threads: %u\n", current_threads[0]);*/
        return -1;
    }

    /* Bit 0 is bit 1(ffsll returns ordinal value) */
    map = map - 1;

    sleep_map_array[0] |= (1ULL << map);
    current_threads[0]++;
    if(current_threads[0] == 62){   /* 62 because we have slots for main slurmctl and slurmd threads */
        printf("SIM ERROR: %d threads is not possible\n", current_threads[0]);
        return -1;
    }

    return map;
}

/* We need to know which threads are from slurmctld and which from slurmd */
int register_program(){

    int fproc;
    char fname[100];
    char *raw;
    char program_name[100];
    int count = 0;

    sprintf(fname, "/proc/%d/cmdline", getpid());
    printf("Register: opening file %s\n", fname);

    fproc = open(fname, O_RDONLY);

    if(fproc < 0){
        printf("We got a problem reading proc file for process %d\n", getpid());
        return -1;
    }

    memset(&program_name, '\0', 100);
    count = read(fproc, &program_name, 100);
    count = strlen(program_name);

    printf("[Read %d bytes] Using program name %s\n", count, &program_name);

    /* Getting last slash position or full program name */
    while(program_name[count] != '/'){
        count--;
        if(count == 0)
            break;
    }

    count++;

    raw = (char *)&program_name + count;

    printf("[Read %d(%d) bytes] Registering program %s and pid %d\n", count, errno, raw, getpid());

    if(strncmp(raw, "slurmctld", 9) == 0){
        printf("Registering slurmctld with pid %d\n", getpid());
        slurmctl_pid[0] = getpid();
    }else{
        printf("Registering slurmd with pid %d\n", getpid());
        slurmd_pid[0] = getpid();
    }

    close(fproc);

    return 0;
}



int init_semaphores(){

    char sem_name[100];

    printf("Initializing semaphores...\n");

    global_sem = sem_open(SIM_GLOBAL_SEM, 0);
    if(global_sem == SEM_FAILED){
        printf("global_sem can not be created\n");
        return -1;
    }

    /* Slurmctld and slurmd main threads are registered during initialization */
    main_thread = pthread_self();

    sem_wait(global_sem);

    main_thread = pthread_self();

    main_thread_id = get_new_thread_id(&main_thread);
    threads_data[main_thread_id].sleep = -1;
    threads_data[main_thread_id].pid  = getpid();

    printf("Getting main_thread_id %d\n", main_thread_id);

    sem_post(global_sem);

    /* and then initializing semaphores for this main thread */
    memset(&sem_name, '\0', 100);
    sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_PREFIX, main_thread_id);
    printf("Opening semaphore %s\n", sem_name);

    thread_sem[main_thread_id] = sem_open(sem_name, 0);
    if(thread_sem[main_thread_id] == SEM_FAILED){
        printf("Error opening semaphore number %d\n", main_thread_id);
        return -1;
    }

    memset(&sem_name, '\0', 100);
    sprintf(sem_name, "%s%d", SIM_LOCAL_SEM_BACK_PREFIX, main_thread_id);
    printf("Opening semaphore %s\n", sem_name);

    thread_sem_back[main_thread_id] = sem_open(sem_name, 0);
    if(thread_sem_back[main_thread_id] == SEM_FAILED){
        printf("Error opening semaphore number %d\n", main_thread_id);
        return -1;
    }

    return 0;
}


/* Slurmctld and slurmd do not really build shared memory but they use that one built by sim_mgr */
int building_shared_memory(){

    int fd;

    fd = shm_open(SLURM_SIM_SHM, O_RDWR, S_IRUSR | S_IWUSR);
    if(fd < 0){
        printf("Error opening %s\n", SLURM_SIM_SHM);
        return -1;
    }

    ftruncate(fd, 8192);

    timemgr_data = mmap(0, 8192, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

    if(!timemgr_data){
        printf("mmaping %s file can not be done\n", SLURM_SIM_SHM);
        return -1;
    }

    /* Initializing pointers to shared memory */
    current_sim = timemgr_data + SIM_SECONDS_OFFSET;
    current_micro = timemgr_data + SIM_MICROSECONDS_OFFSET;
    threads_data = timemgr_data + SIM_SYNC_ARRAY_OFFSET;
    current_threads = timemgr_data + SIM_THREADS_COUNT_OFFSET;
    proto_threads = timemgr_data + SIM_PROTO_THREADS_OFFSET;
    sleep_map_array = timemgr_data + SIM_SLEEP_ARRAY_MAP_OFFSET;
    thread_exit_array = timemgr_data + SIM_THREAD_EXIT_MAP_OFFSET;
    thread_new_array = timemgr_data + SIM_THREAD_NEW_MAP_OFFSET;
    fast_threads_counter = timemgr_data + SIM_FAST_THREADS_OFFSET;
    pthread_create_counter = timemgr_data + SIM_PTHREAD_CREATE_COUNTER;
    pthread_exit_counter = timemgr_data + SIM_PTHREAD_EXIT_COUNTER;
    slurmctl_pid = timemgr_data + SIM_PTHREAD_SLURMCTL_PID;
    slurmd_pid = timemgr_data + SIM_PTHREAD_SLURMD_PID;

    if((slurmctl_pid[0] == 0) || (slurmd_pid[0] == 0)){

        register_program();

        if(init_semaphores() < 0){
            printf("semaphores initialization failed\n");
            return 1;
        }
    }

    return 0;
}

int getting_simulation_users(){

    int fich;
    char c;
    int pos;
    char username[100];
    char uid_string[10];
    uid_t sim_uid;
    sim_user_info_t *new_sim_user;

    if(sim_users_list)
        return 0;

    fich = open("users.sim", O_RDONLY);
    if(fich < 0){
        printf("ERROR: no users.sim available\n");
        return -1;
    }

#ifdef DEBUG
    printf("Starting reading users...\n");
#endif

    while(1){
        
        memset(&username, '\0', 100);
        pos = 0;

        while(read(fich, &c, 1) > 0){
            username[pos] = c;
            if(username[pos] == ':'){
                username[pos] = '\0';
                break;
            }
            pos++;
        }

        if(pos == 0)
            break;

        new_sim_user = malloc(sizeof(sim_user_info_t));
        if(new_sim_user == NULL){
            printf("Malloc error for new sim user\n");
            return -1;
        }
#ifdef DEBUG
        printf("Reading user %s\n", username);
#endif
        new_sim_user->sim_name = strdup(username);

        pos = 0;
        memset(&uid_string, '\0', 10);

        while(read(fich, &c, 1) > 0){
            uid_string[pos] = c;
            if(uid_string[pos] == '\n'){
                uid_string[pos] = '\0';
                break;
            }
            pos++;
        }
#ifdef DEBUG
        printf("Reading uid %s\n", uid_string);
#endif

        new_sim_user->sim_uid = (uid_t)atoi(uid_string);

        /* Inserting as list head */
        new_sim_user->next = sim_users_list;
        sim_users_list = new_sim_user;

    }

}

/* TODO: It is necessary to use another method for finding out which are the paths to libraries 
 * * and, maybe, which are the libraries (it seems it is not always the same libraries for different 
 * * distributions) */

void __attribute__ ((constructor)) sim_init(void){

    void *handle;
#ifdef DEBUG
    sim_user_info_t *debug_list;
#endif

    if(building_shared_memory() < 0){
        printf("Error building shared memory and mmaping it\n");
    };


   if(getting_simulation_users() < 0){
       printf("Error getting users information for simulation\n");
   }

#ifdef DEBUG
   debug_list = sim_users_list;
   while(debug_list){
       printf("User %s with uid %u\n", debug_list->sim_name, debug_list->sim_uid);
       debug_list = debug_list->next;
   }
#endif

    if(real_gettimeofday == NULL){

        //printf("Looking for real gettimeofday function\n");
        handle = dlopen(LIBC_PATH, RTLD_LOCAL | RTLD_LAZY);
        if(handle == NULL){
            printf("Error in dlopen %s\n", dlerror());
            return;
        }

        real_gettimeofday = dlsym( handle, "gettimeofday");
        if(real_gettimeofday == NULL){
            printf("Erro: no sleep function found\n");
            return;
        }
    }

    /* slurmctld and slurmd got all the wrappers but some other programs like sinfo or scontrol just time related wrappers */
    if(slurmctl_pid[0] == getpid() || 
            slurmd_pid[0] == getpid())
    {
        printf("This slurm program is not the controller nor a slurmd (%d)(controller: %d)(daemon: %d)\n", getpid(), slurmctl_pid[0], slurmd_pid[0]);
        if(real_sleep == NULL){

            printf("Looking for real sleep function\n");
            handle = dlopen(LIBC_PATH, RTLD_LOCAL | RTLD_LAZY);
            if(handle == NULL){
                printf("Error in dlopen\n");
                return;
            }
            real_sleep = dlsym( handle, "sleep");
            if(real_sleep == NULL){
                printf("Erro: no sleep function found\n");
                return;
            }
        }
    }

    if(real_time == NULL){

        //printf("Looking for real time function\n");
        handle = dlopen(LIBC_PATH, RTLD_LOCAL | RTLD_LAZY);
        if(handle == NULL){
            printf("Error in dlopen: %s\n", dlerror());
            return;
        }
        real_time = dlsym( handle, "time");
        if(real_time == NULL){
            printf("Erro: no sleep function found\n");
            return;
        }
    }

    if(slurmctl_pid[0] == getpid() || 
            slurmd_pid[0] == getpid())
    {
        if(real_pthread_create == NULL){

            printf("Looking for real pthread_create function\n");
            handle = dlopen(LIBPTHREAD_PATH, RTLD_LOCAL | RTLD_LAZY);
            if(handle == NULL){
                printf("Error in dlopen: %s\n", dlerror());
                return;
            }
            real_pthread_create = dlsym( handle, "pthread_create");
            if(real_pthread_create == NULL){
                printf("Erro: no pthread_create function found\n");
                return;
            }
        }

        if(real_pthread_exit == NULL){

            printf("Looking for real pthread_exit function\n");
            handle = dlopen(LIBPTHREAD_PATH, RTLD_LOCAL | RTLD_LAZY);
            if(handle == NULL){
                printf("Error in dlopen: %s\n", dlerror());
                return;
            }
            real_pthread_exit = dlsym( handle, "pthread_exit");
            if(real_pthread_exit == NULL){
                printf("Erro: no pthread_exit function found\n");
                return;
            }
        }

        if(real_pthread_join == NULL){

            printf("Looking for real pthread_join function\n");
            handle = dlopen(LIBPTHREAD_PATH, RTLD_LOCAL | RTLD_LAZY);
            if(handle == NULL){
                printf("Error in dlopen: %s\n", dlerror());
                return;
            }
            real_pthread_join = dlsym( handle, "pthread_join");
            if(real_pthread_join == NULL){
                printf("Erro: no pthread_join function found\n");
                return;
            }
        }

        /* Telling sim_mgr we are ready for the kick off */
        sem_wait(global_sem);
        thread_exit_array[0]++;
        sem_post(global_sem);

        /* And wait for starting */
        sem_wait(thread_sem[main_thread_id]);
    }

    //printf("sim_init: done\n");
}


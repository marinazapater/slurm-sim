#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <errno.h>
#include <signal.h>

#define SIM_MGR_SERVER_PORT 23720

#define mysql_client "get_sim_data.pl"

int alert_received = 0;

void alert_checked(int signum){
	printf("OK. Resuming operation\n");
	alert_received = 1;
}

int main(int argc, char *argv[], char *envp[]){


	int sock_control;
	struct sockaddr_in mgr_addr;
	int req_type;

	if(argc < 2){
		printf("Usage: %s req_type(100=control_thread, 200=get_thread_id)\n", argv[0]);
		return -1;
	}

	/* When started before than exec_sim inside a global script test */
	sleep(5);

	sock_control = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if(sock_control < 0){
		printf("sim_ctrl: error with socket\n");
		return -1;
	}

	mgr_addr.sin_family = AF_INET;
	mgr_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	mgr_addr.sin_port = htons(SIM_MGR_SERVER_PORT);

	if(connect(sock_control, (struct sockaddr *)&mgr_addr, sizeof(mgr_addr)) < 0){
		printf("sim_ctrl: error connecting to simulation manager: %d\n", errno);
		return -1;
	}

	req_type = atoi(argv[1]);


       	signal(SIGUSR1, alert_checked);

	switch(req_type){

	       	int thread_id;
	       	int res;
	       	unsigned long long func_addr;
		unsigned int current_sim, real_time, real_time_usecs;
		unsigned int last_secs, last_usecs;
	       	int count;

		case 100:
		       
			if(argc < 3){
				printf("Request 100 needs a parameter for thread id\n");
				return -1;
			}
			thread_id = atoi(argv[2]);

			printf("Sending request 100\n");
		       	if(send(sock_control, &req_type, sizeof(req_type), 0) < 0){
			       	printf("sim_ctrl: send error\n");
			       	return -1;
		       	}

			printf("Sending thread id %d\n", thread_id);
		       	if(send(sock_control, &thread_id, sizeof(req_type), 0) < 0){
			       	printf("sim_ctrl: send error\n");
			       	return -1;
		       	}
			printf("Waiting for confirmation...");
		       	if(recv(sock_control, &res, sizeof(req_type), 0) < 0){
			       	printf("sim_ctrl: recv error\n");
			       	return -1;
		       	}
			if(res == 0)
				printf("Ok. Now simulation is frozen when thread %d is ready to be executed\n", thread_id);
			else{
				printf("ERROR. This can not be done. Are you using the right thread id?\n");
				return -1;
		       	}
			break;

		case 200:

			printf("Sending request 200\n");
		       	if(send(sock_control, &req_type, sizeof(req_type), 0) < 0){
			       	printf("sim_ctrl: send error\n");
			       	return -1;
		       	}
		       	res = 0;

			while(res > 0){
			       	if(recv(sock_control, &func_addr, sizeof(func_addr), 0) < 0){
				       	printf("sim_ctrl: recv error\n");
				       	return -1;
			       	}
				printf("Thread ID: %d, func_addr: %016llx\n", res, func_addr);
				res--;
			}
			break;

		case 300:

			last_secs = 0;
			last_usecs = 0;

			count = 0;

			while(1){
				int child;
				int exec_result;
				char *child_args[5] = {"","","","", (char *)0 };
				unsigned int diff;

			       	//printf("Getting current simulation time\n");
				/* this is the signal for getting simulation statistics safely */
			       	if(recv(sock_control, &current_sim, sizeof(current_sim), 0) < 0){
				       	printf("sim_ctrl: recv error\n");
				       	return -1;
			       	}

				//printf("Current simulation time %u\n", current_sim);
			       	if(recv(sock_control, &real_time, sizeof(real_time), 0) < 0){
				       	printf("sim_ctrl: recv error\n");
				       	return -1;
			       	}


			       	if(recv(sock_control, &real_time_usecs, sizeof(real_time), 0) < 0){
				       	printf("sim_ctrl: recv error\n");
				       	return -1;
			       	}

				//printf("Real simulation time %u-%u\n", real_time, real_time_usecs);

				if(last_secs > 0){
					if(real_time_usecs > last_usecs)
						diff = ((real_time - last_secs) * 1000000) + (real_time_usecs - last_usecs);
					else
						diff = ((real_time - last_secs) * 1000000) + (real_time_usecs + (1000000 - last_usecs));
				}else{
					diff = 0;
				}

				last_secs = real_time;
				last_usecs = real_time_usecs;


				/* Let's work with milliseconds */
				//printf("Diff: %u (%u)\n", diff, diff / 1000);
				diff = diff / 1000;

			       	child = fork();

			       	if(child == 0){ /* the child */

					if(daemon(1,1) < 0){
						printf("daemon call error\n");
					};
					child_args[1] = malloc(30);
					memset(child_args[1], '\0', 30);
					child_args[2] = malloc(30);
					memset(child_args[2], '\0', 30);
					child_args[3] = malloc(30);
					memset(child_args[3], '\0', 30);

					sprintf(child_args[1], "%u", current_sim);
					sprintf(child_args[2], "%u", diff);
					sprintf(child_args[3], "%d", count);

					count++;

				       	if(execve(mysql_client, child_args, envp) < 0){
					       	printf("Error in execve for tch\n");
					       	printf("Exiting...\n");
					       	return(-1);
				       	}
			       	}

#if 0
				if(current_sim >= (1305530000)){
				//if(current_sim >= (1304958545- 300)){
					while(alert_received == 0){
						printf("ALERT! We got the 1305913406 second\n");
						sleep(5);
					}
				}
#endif

				waitpid(child, &exec_result, 0);

			       	//printf("Returning control to sim_mgr...\n");
			       	if(send(sock_control, &current_sim, sizeof(current_sim), 0) < 0){
				       	printf("sim_ctrl: recv error\n");
				       	return -1;
			       	}
				count++;
			}

			break;

	}

	close(sock_control);

	return 0;

}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <time.h>

// Author: Dat Nguyen
// Date: 04/17/2025
// This program, user.c, is the user process that waits for a message from the main oss system then simulates doing work in the system, randomly terminating at times. Sending a message back to the oss about time spent.

#define MSG_KEY 864049
#define RANDOM_TERMINATION 50 // Constant for termination of child.

typedef struct ossMSG { // Message structure
	long mtype;
	int msg;
} ossMSG;

int main(int argc, char **argv) { // Main program
	
	int quantum = 0;
	int timeConsumed = 0;
	
	srand(time(NULL) * getpid()); // Randomize, since child process may have the same seed, we must do this to ensure that the processes are independent when using rand.

	int msgid = msgget(MSG_KEY, 0666); // Get message from shared memory
	if (msgid == -1) {
        	printf("Error: User msgget failed. \n");
        	exit(1);
    	}

	while (1) { // Main loop
		// Receive msg
		ossMSG receiveMSG;
		if (msgrcv(msgid, &receiveMSG, sizeof(int), getpid(), 0) == -1) {
			printf("Error: User msgrcv failed. \n");
			exit(1);
	       	}

		quantum = receiveMSG.msg;
		int termNum = rand() % 100; // Generate rand to see if process will terminate
		if (termNum < RANDOM_TERMINATION) { // If the process does decide to terminate.
			int workTime = 1 + rand()% 99; // How much of the quantum time it uses
			timeConsumed = (quantum * workTime) / 100; // How much time is used overall
			// Send message to OSS     
			ossMSG sendMSG;
                	sendMSG.mtype = getppid();
                	sendMSG.msg = -timeConsumed;
         
			if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
                        	printf("Error: User msgsnd failed. \n");
                        	exit(1);
                	}
			break;
		}
        	
        	if (rand() % 2 == 0) { // Work with partial time and get blocked
			int workTime = 1 + rand() % 99;
                        timeConsumed = (quantum * workTime) / 100;
		}
        	else { // Use full time
			timeConsumed = quantum;
        	}

        	// Send message to OSS
        	ossMSG sendMSG;
        	sendMSG.mtype = getppid();
        	sendMSG.msg = timeConsumed;
        	if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
                	printf("Error: User msgsnd failed. \n");
                	exit(1);
        	}
		
		if (timeConsumed >= 50000000) { // Terminate if take too long.
   			 sendMSG.mtype = getppid();
		     	 sendMSG.msg = -timeConsumed; 
		     	 msgsnd(msgid,&sendMSG,sizeof(int),0);
		     	 break;
		}
	}
	return 0;
}

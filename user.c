#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <time.h>

#define MSG_KEY 864049
#define RANDOM_TERMINATION 5 // Constant for termination of child.

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

	// Receive message from OSS
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
        	sendMSG.mtype = 1;
        	sendMSG.msg = -timeConsumed;
        	if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
        		printf("Error: User msgsnd failed. \n");
			exit(1);
		}
		return 0;
	}
	// Roll to decide between using full quantum or blocking
    	int fullTimeConsumed = rand() % 2;
    	if (fullTimeConsumed) {
        	timeConsumed = quantum;
	} 
	else { // Use time, will be interpreted as interrupt in OSS when timeConsumed is sent to OSS.
        	int workTime = 1 + rand() % 99;
        	timeConsumed = (quantum * workTime) / 100;
    	}

	// Send message to OSS
        ossMSG sendMSG;
        sendMSG.mtype = 1;
        sendMSG.msg = timeConsumed;
        if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
		printf("Error: User msgsnd failed. \n");
                exit(1);
	}

	return 0;
}

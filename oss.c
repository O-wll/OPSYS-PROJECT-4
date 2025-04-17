#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/msg.h>
#include <sys/shm.h> // For shared memory
#include <sys/ipc.h> // Also for shared memory, allows worker class to access shared memory
#include <time.h>
#include <string.h> // For memset

#define SHM_KEY 855049
#define PCB_KEY 866150
#define MSG_KEY 864049
#define MAX_PCB 18
#define MAX_PROC 20
#define NANO_TO_SEC 1000000000
#define BASE_QUANTUM_NANO 10000000

const int maxTimeBetweenNewProcsSecs = 1;
const long maxTimeBetweenNewProcsNano = 100000000;

// Variables for our queue system.
int highQueue[MAX_PROC];
int highHead = 0, highTail = 0;
int midQueue[MAX_PROC];
int midHead = 0, midTail = 0;
int lowQueue[MAX_PROC];
int lowHead = 0, lowTail = 0;

// Using a structure for our simulated clock, storing seconds and nanoseconds.
typedef struct SimulatedClock {
       unsigned int seconds;
       unsigned int nanoseconds;
} SimulatedClock;

typedef struct PCB {
	int occupied;
	pid_t pid;
	int startSeconds;
	int startNano;
	int serviceTimeSeconds;
	int serviceTimeNano;
	int eventWaitSec;
	int eventWaitNano;
	int blocked;
} PCB;
PCB processTable[MAX_PCB];

typedef struct ossMSG {
	long mtype;
	int msg;
} ossMSG;

void incrementClock(SimulatedClock *clock, int addSec, int addNano);

int main(int argc, char **argv) {

	int totalForked = 0;
	unsigned int nextSecFork = 0;
	unsigned int nextSecNano = 0;

	srand(time(NULL));

	int shmid = shmget(SHM_KEY, sizeof(SimulatedClock), IPC_CREAT | 0666); // Creating shared memory using shmget.
	if (shmid == -1) { // If shmid is -1 as a result of shmget failing and returning -1, error message will print.
        	printf("Error: OSS shmget failed. \n");
        	exit(1);
    	}
	
	SimulatedClock *clock = (SimulatedClock *)shmat(shmid, NULL, 0); // Attach shared memory, clock is now a pointer to SimulatedClock structure.
	if (clock == (void *)-1) { // if shmat, the attaching shared memory function, fails, it returns an invalid memory address.
		printf("Error: OSS shared memory attachment failed. \n");
		exit(1);
	}

	// Initialize clock.
	clock->seconds = 0;
	clock->nanoseconds = 0;

	memset(processTable, 0, sizeof(processTable)); // Allocating memory.

	int msgid = msgget(MSG_KEY, IPC_CREAT | 0666); // Setting up msg queue.
	if (msgid == -1) {
		printf("Error: OSS msgget failed. \n");
		exit(1);
	}

	for (int i = 0; i < MAX_PROC; ++i) {
        	highQueue[i] = -1;
        	midQueue[i] = -1;
        	lowQueue[i] = -1;
	}

	return 0;
}

void incrementClock(SimulatedClock *clock, int addSec, int addNano) { // This function simulates the increment of our simulated clock.
	clock->seconds += addSec;
	clock->nanoseconds += addNano;

	while (clock->nanoseconds >= NANO_TO_SEC) {
		clock->seconds++;
        	clock->nanoseconds -= NANO_TO_SEC;
    }
}

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
#define MAX_PROC 18
#define MAX_PROC_TOTAL 20
#define NANO_TO_SEC 1000000000

// Using a structure for our simulated clock, storing seconds and nanoseconds.
typedef struct SimulatedClock {
        int seconds;
        int nanoseconds;
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
struct PCB processTable[20];

typedef struct ossMSG {
	long mtype;
	int msg;
} ossMSG;

void incrementClock(SimulatedClock *clock, int currentProc);

int main(int argc, char **argv) {
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

	int pcbid = shmget(PCB_KEY, sizeof(PCB) * MAX_PROC, IPC_CREAT | 0666); // Creating shared memory using shmget for PCB.
	if (pcbid == -1) { // Check for shmget error.
		printf("Error: OSS shmget failed. \n");
		exit(1);
	}

	PCB *pcbTable = (PCB *) shmat(pcbid, NULL, 0); // Attatching shared memory for PCB table.
	if (pcbTable == (void *)-1) { // Check for shmat error.
		printf("Error: OSS shmat failed. \n");
		exit(1);
	}

	memset(processTable, 0, sizeof(PCB) * MAX_PROC);

	return 0;
}

void incrementClock(SimulatedClock *clock, int currentProc) { // This function simulates the increment of our simulated clock.

	if (currentProc > 0) {
		clock->nanoseconds += (250 * 1000000) / currentProc;
	} else {
		clock->nanoseconds += (250 * 1000000);
	}

	while (clock->nanoseconds >= NANO_TO_SEC) { // Expect even after reducing nano seconds to have a bit of remaining nano seconds, while ensures that if they build up, that they'll be reduced properly.
		clock->seconds++;
		clock->nanoseconds -= NANO_TO_SEC;
	}
}

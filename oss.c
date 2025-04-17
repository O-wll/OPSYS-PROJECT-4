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

typedef struct PCB { // PCB structure
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

// Message system
typedef struct ossMSG {
	long mtype;
	int msg;
} ossMSG;

void incrementClock(SimulatedClock *clock, int addSec, int addNano); // Clock increment

int main(int argc, char **argv) {

	int totalForked = 0;
	pid_t pid;
	unsigned int nextSecFork = 0;
	unsigned int nextNanoFork = 0;
	int activeProcs = 0;

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
	
	// Initialize the queues
	for (int i = 0; i < MAX_PROC; ++i) { 
        	highQueue[i] = -1;
        	midQueue[i] = -1;
        	lowQueue[i] = -1;
	}
	
	// Get next random times to fork.
	nextSecFork = clock->seconds + (rand() % (maxTimeBetweenNewProcsSecs + 1));
    	nextNanoFork = clock->nanoseconds + (rand() % maxTimeBetweenNewProcsNano);
    	
	// Convert nano second to second if necessary.
	if (nextNanoFork >= NANO_TO_SEC) {
        	nextSecFork++;
        	nextNanoFork -= NANO_TO_SEC;
    	}

	while (totalForked < MAX_PROC || activeProcs > 0) { // Main loop
		// Unblock any blocked processes if their wait event is up.
		for (int i = 0; i < MAX_PCB; i++) {
			if (processTable[i].occupied && processTable[i].blocked) { // Check for if process is occupied and blocked.
				// Unblock the process if wait time is up
				int ready = 0;
				if (clock->seconds > processTable[i].eventWaitSec) {
                			ready = 1;
				}
				else if (clock->seconds == processTable[i].eventWaitSec && clock->nanoseconds >= processTable[i].eventWaitNano) {
					ready = 1;
				}

				if (ready) { // If wait time is up, unblock process and re enter into high priority queue
					processTable[i].blocked = 0;
                			highQueue[highTail] = i;      
                			highTail = (highTail + 1) % MAX_PROC;
				}
			}
		}

	       	if ((clock->seconds > nextSecFork) || (clock->seconds == nextSecFork && clock->nanoseconds >= nextNanoFork)) { // Fork child
			// Finding a free PCB slot
			int freePCB = -1;
			for (int i = 0; i < MAX_PCB; i++) {
				if (!processTable[i].occupied) {
					freePCB = i;
					break;
                		}	
			}
			
			if (freePCB != -1 && totalForked < MAX_PROC) { // Safe guarding against forking anymore children
				pid = fork();

				if (pid < 0) {
					printf("Error: fork failed. \n");
					exit(1);
				}

				if (pid == 0) {
					execl("./user", "./user", NULL);
						
				}
				// Filling PCB entries 
				processTable[freePCB].occupied = 1;
                		processTable[freePCB].pid = pid;
                		processTable[freePCB].startSeconds = clock->seconds;
                		processTable[freePCB].startNano = clock->nanoseconds;
                		processTable[freePCB].blocked = 0;

				// High Priority Queue
				highQueue[highTail] = freePCB;
				highTail = (highTail + 1) % MAX_PROC; // When high tail reaches the max process, it'll go back to 0.

				totalForked++;
				activeProcs++;
			}
			
			// Get new fork times
			nextSecFork = clock->seconds + (rand() % (maxTimeBetweenNewProcsSecs + 1));
			nextNanoFork = clock->nanoseconds + (rand() % maxTimeBetweenNewProcsNano);
			// Conversion
			if (nextNanoFork >= NANO_TO_SEC) {
				nextSecFork++;
				nextNanoFork -= NANO_TO_SEC;
			}
		}

		int pcbIndex = -1;
		int currentQueueLevel = -1;

		// Pulling from queue, if high queue is empty, grab from mid queue, then low queue.
		if (highHead != highTail) {
            		pcbIndex = highQueue[highHead];
            		highHead = (highHead + 1) % MAX_PROC;
            		currentQueueLevel = 0;
        	} 
		else if (midHead != midTail) {
            		pcbIndex = midQueue[midHead];
            		midHead = (midHead + 1) % MAX_PROC;
            		currentQueueLevel = 1;
        	} 
		else if (lowHead != lowTail) {
            		pcbIndex = lowQueue[lowHead];
            		lowHead = (lowHead + 1) % MAX_PROC;
            		currentQueueLevel = 2;
        	}

		if (pcbIndex == -1) { // If no active process, increment clock.
			unsigned int addSec = 0;
			unsigned int addNano = 0;

			if (nextNanoFork < clock->nanoseconds) {
				addSec = nextSecFork - clock->seconds - 1;
				addNano = (NANO_TO_SEC - clock->nanoseconds) + nextNanoFork;
			}
			else {
				addSec = nextSecFork - clock->seconds;
				addNano = nextNanoFork - clock->nanoseconds;
				incrementClock(clock, addSec, addNano);
				continue; // Skip to next loop
			}
		}
		
		// Increasing quantum based on queue priority.
		int quantum = BASE_QUANTUM_NANO;
		if (currentQueueLevel == 1) {
			quantum *= 2;
		}
		else if (currentQueueLevel == 2) {
			quantum *= 4;
		}
		
		// Simulate the time it takes to schedule.
		incrementClock(clock, 0, 100 + rand() % 9901);

		// Send message to user process
		ossMSG sendMSG;
		sendMSG.mtype = processTable[pcbIndex].pid;
		sendMSG.msg = quantum;
		
		if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
			printf("Error: OSS msgsend failed. \n");
			exit(1);
		}

		// Receive msg from user process
		ossMSG receiveMSG;
		if (msgrcv(msgid, &receiveMSG, sizeof(int), getpid(), 0) == -1) {
			printf("Error: OSS msgrcv failed. \n");
			exit(1);
		}
		// How much time has a user process consumed
		int timeConsumed = receiveMSG.msg;
		if (timeConsumed < 0) { // If receiving msg and time is negative, revert it to positive so we can increment clock.
			timeConsumed = -timeConsumed;
		}
		
		// Increment clock
		incrementClock(clock, 0, timeConsumed);

		// Update PCB table 
	        processTable[pcbIndex].serviceTimeNano += timeConsumed;
        	while (processTable[pcbIndex].serviceTimeNano >= NANO_TO_SEC) { // Convert to seconds if necessary.
            		processTable[pcbIndex].serviceTimeSeconds++;
            		processTable[pcbIndex].serviceTimeNano -= NANO_TO_SEC;
        	}

		// If user process is terminating
		if (receiveMSG.msg < 0) {
			waitpid(processTable[pcbIndex].pid, NULL, 0);
            		memset(&processTable[pcbIndex], 0, sizeof(PCB)); // Data erased since process is terminated, frees up slot.
            		activeProcs--;
		}
		else { // If user process is not terminating
			if (receiveMSG.msg == quantum) { // If process uses full quantum.
				if (currentQueueLevel == 0) { // Demote to mid queue
					midQueue[midTail] = pcbIndex;
                    			midTail = (midTail + 1) % MAX_PROC;
				}
				else if (currentQueueLevel == 1) {
					lowQueue[lowTail] = pcbIndex;
					lowTail = (lowTail + 1) % MAX_PROC;
				}
			}
			else { // Block Process for I/O
				processTable[pcbIndex].blocked = 1;

				// Simulate some random amount of time to wait.
				int waitTime = 5000000 + rand() % 5000000;
    				processTable[pcbIndex].eventWaitSec = clock->seconds;
    				processTable[pcbIndex].eventWaitNano = clock->nanoseconds + waitTime;

				if (processTable[pcbIndex].eventWaitNano >= NANO_TO_SEC) { // Conversion if necessary 
					processTable[pcbIndex].eventWaitSec++;
        				processTable[pcbIndex].eventWaitNano -= NANO_TO_SEC;
				} 
			}
		}
	}
	// Detach shared memory
    	if (shmdt(clock) == -1) {
        	printf("Error: OSS Shared memory detachment failed \n");
		exit(1);
    	}	

    	// Remove shared memory
    	if (shmctl(shmid, IPC_RMID, NULL) == -1) {
        	printf("Error: Removing memory failed \n");
		exit(1);
    	}
	
	if (msgctl(msgid, IPC_RMID, NULL) == -1) {
		printf("Error: Removing msg queue failed. \n");
		exit(1);
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

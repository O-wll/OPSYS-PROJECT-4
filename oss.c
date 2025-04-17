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
void signalHandler(int sig);

int main(int argc, char **argv) { // Variables

	int totalForked = 0;
	int pcbIndex = -1;
	pid_t pid;
	unsigned int nextSecFork = 0;
	unsigned int nextNanoFork = 0;
	int activeProcs = 0;
	// For our log file/output
	FILE *file;
	int logLines = 0;
	unsigned int totalWaitTime = 0;
	unsigned int totalCPUTime = 0;
	unsigned int totalBlockedTime = 0;
	unsigned int idleTime = 0;
	double waitAvg = 0.0;
	double blockAvg = 0.0;
	double cpuUsage = 0.0;
	// For checking how much time has passed.
	SimulatedClock lastLogged;

	file = fopen("oss.log", "w"); // Open log file
	if (!file) {
		printf("Error: failed opening log file. \n");
		exit(1);
	}
	// Initialize clock structure
	lastLogged.seconds = 0;
	lastLogged.nanoseconds = 0;

	srand(time(NULL));

	// Start alarm and set signal.
	alarm(60);
	signal(SIGINT, signalHandler);
	signal(SIGALRM, signalHandler);

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
					// Update wait time
					processTable[pcbIndex].startSeconds = clock->seconds;
				    	processTable[pcbIndex].startNano = clock->nanoseconds;
					
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
				
				// Log making of new user process
				if (logLines < 10000) {
					fprintf(file, "OSS: Generating process with PID %d and putting it in queue 0 at time %u:%u\n", pid, clock->seconds, clock->nanoseconds);
					logLines++;
				}
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
			}
			incrementClock(clock, addSec, addNano);
			// Log idle time of CPU
			idleTime += addNano;
			
			if (logLines < 10000) {
				fprintf(file, "OSS: CPU idle at time %u:%u\n", clock->seconds, clock->nanoseconds);
				logLines++;
			}

			continue; // Skip to next loop
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

		// Calculate wait time of processes.
		unsigned int dispatchWaitTime = (clock->seconds - processTable[pcbIndex].startSeconds) * NANO_TO_SEC + (clock->nanoseconds - processTable[pcbIndex].startNano);
		totalWaitTime += dispatchWaitTime;

		// Send message to user process
		ossMSG sendMSG;
		sendMSG.mtype = processTable[pcbIndex].pid;
		sendMSG.msg = quantum;
		
		if (msgsnd(msgid, &sendMSG, sizeof(int), 0) == -1) {
			printf("Error: OSS msgsend failed. \n");
			exit(1);
		}
		
		// Log OSS giving time slice 
		if (logLines < 10000) {
			fprintf(file, "OSS: Dispatching process with PID %d from queue %d at time %u:%u,\n", processTable[pcbIndex].pid, currentQueueLevel, clock->seconds, clock->nanoseconds);
			logLines++;
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
		
		// Log total time for dispatch 
		if (logLines < 10000) {
			fprintf(file, "OSS: total time this dispatch was %d nanoseconds\n", timeConsumed);
			fprintf(file, "OSS: Receiving that process with PID %d ran for %d nanoseconds\n", processTable[pcbIndex].pid, timeConsumed);
			logLines += 2;
		}

		// Log total time for CPU time usage
		totalCPUTime += timeConsumed;

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
					// Update wait time
					processTable[pcbIndex].startSeconds = clock->seconds;
					processTable[pcbIndex].startNano    = clock->nanoseconds;
					
					midQueue[midTail] = pcbIndex;
                    			midTail = (midTail + 1) % MAX_PROC;
				}
				else if (currentQueueLevel == 1) {
					// Update wait time
					processTable[pcbIndex].startSeconds = clock->seconds;
					processTable[pcbIndex].startNano = clock->nanoseconds;

					lowQueue[lowTail] = pcbIndex;
					lowTail = (lowTail + 1) % MAX_PROC;
				}
			}
			else { // Block Process for I/O
				processTable[pcbIndex].blocked = 1;

				// Simulate some random amount of time to wait.
				int waitTime = 5000000 + rand() % 5000000;
				totalBlockedTime += waitTime; // Add the wait time to totalBlockedTime.
    				processTable[pcbIndex].eventWaitSec = clock->seconds;
    				processTable[pcbIndex].eventWaitNano = clock->nanoseconds + waitTime;

				if (processTable[pcbIndex].eventWaitNano >= NANO_TO_SEC) { // Conversion if necessary 
					processTable[pcbIndex].eventWaitSec++;
        				processTable[pcbIndex].eventWaitNano -= NANO_TO_SEC;
				} 
			}
		}
		// Logic to check to see how much time has passed so we can output info at correct interval of 0.5 seconds.
		unsigned int secsPassed = clock->seconds - lastLogged.seconds;
		unsigned int nanosPassed = 0;
		if (clock->nanoseconds >= lastLogged.nanoseconds) { // If nanoseconds does not exceed 1 second 
			nanosPassed = clock->nanoseconds - lastLogged.nanoseconds;
		}
		else { // If nanoseconds DOES exceed one second
			nanosPassed = NANO_TO_SEC - lastLogged.nanoseconds + clock->nanoseconds;
		}

		// Check to see if it's been 0.5 seconds simulated time.
		if (secsPassed > 0 || nanosPassed >= 500000000) { 
			 fprintf(file, "\nOSS: Process Table at time %u:%u\n", clock->seconds, clock->nanoseconds); // If so, output process table.
			 for (int i = 0; i < MAX_PCB; i++) {
				 if (processTable[i].occupied) { // Print info about the PCB slot.
					 if (logLines < 10000) {
					 	fprintf(file, "PCB[%d]: PID=%d, Blocked=%d, ServiceTime=%d:%d\n", i, processTable[i].pid, processTable[i].blocked, processTable[i].serviceTimeSeconds, processTable[i].serviceTimeNano);
					 	logLines++;
					 }
				 }
			 }

			 // Print about the log queues 
			 if (logLines < 10000) {
			 	fprintf(file, "Queues: High[H=%d T=%d], Mid[H=%d T=%d], Low[H=%d T=%d]\n", highHead, highTail, midHead, midTail, lowHead, lowTail);
			 	logLines++;
			 }

			 // Keep track of time.
		       	 lastLogged.seconds = clock->seconds;
		  	 lastLogged.nanoseconds = clock->nanoseconds;
		}
	}
        // Summary of program
        unsigned int totalRunTime = clock->seconds * NANO_TO_SEC + clock->nanoseconds; // Calculate total time the program has been running.

        if (totalForked > 0) { // Calculate average wait and block time per process
                waitAvg = (double)totalWaitTime / totalForked;
                blockAvg = (double)totalBlockedTime / totalForked;
        }

        if (totalRunTime > 0) { // Calculating how much of the CPU was used
                cpuUsage = ( (double)totalCPUTime / totalRunTime ) * 100;
        }

        // Output final summary statistics
        fprintf(file, "\n Simulation Summary \n");
        fprintf(file, "Average wait time: %.2f ns\n", waitAvg);
        fprintf(file, "Average blocked time: %.2f ns\n", blockAvg);
        fprintf(file, "CPU utilization: %.2f%%\n", cpuUsage);
        fprintf(file, "Idle CPU time: %u ns\n", idleTime);

        fclose(file);

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
	
	// Remove message queue
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

void signalHandler(int sig) { // Signal handler
       	// Catching signal
	if (sig == SIGALRM) { // 60 seconds have passed
	       	fprintf(stderr, "Alarm signal caught, terminating all processes.\n");
       	}
     	else if (sig == SIGINT) { // Ctrl-C caught
	       	fprintf(stderr, "Ctrl-C signal caught, terminating all processes.\n");
       	}

	for (int i = 0; i < MAX_PCB; i++) { // Kill all processes.
		if (processTable[i].occupied) {
			kill(processTable[i].pid, SIGTERM);
	    	}
	}

	// Cleanup shared memory
    	int shmid = shmget(SHM_KEY, sizeof(SimulatedClock), 0666);
    	if (shmid != -1) {
		SimulatedClock *clock = (SimulatedClock *)shmat(shmid, NULL, 0);
		if (clock != (void *)-1) { // Detach shared memory
			if (shmdt(clock) == -1) {
				printf("Error: OSS Shared memory detachment failed \n");
				exit(1);
			}
		}
		// Remove shared memory
		if (shmctl(shmid, IPC_RMID, NULL) == -1) {
                	printf("Error: Removing memory failed \n");
                	exit(1);
        	}
	}

	// Cleanup message queue
    	int msgid = msgget(MSG_KEY, 0666);
    	if (msgid != -1) {
		if (msgctl(msgid, IPC_RMID, NULL) == -1) {
		    	printf("Error: Removing msg queue failed. \n");
		       	exit(1);
	       	}
       	}

	exit(1);
}

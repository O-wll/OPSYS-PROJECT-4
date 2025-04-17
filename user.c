#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/msg.h>
#include <time.h>

#define MSG_KEY 864049
#define RANDOM_TERMINATION 5

typedef struct ossMSG {
	long mtype;
	int msg;
} ossMSG;

int main(int argc, char **argv) {
	return 0;
}

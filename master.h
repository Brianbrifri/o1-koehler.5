#ifndef MASTER_H
#define MASTER_H
#include <ctype.h>
#include <sys/wait.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>
#include <stdbool.h>
#include "struct.h"


void spawnSlave(void);
bool isTimeToSpawn(void);
void setTimeToSpawn(void);
int incrementTimer(void);
void updateAverageTurnaroundTime(int);
int processMessageQueue(void);
void performActionsFromMessage(int);
void setupResources(void);
void resourceSnapshot(void);
void performResourceRequest(int, int);
void performResourceRelease(int, int);
void performProcessCleanup(int);
int deadlock(void);
int reqLtAvail(int*, int);
void killAProcess(void);
void checkAndProcessRequests(void); 
void interruptHandler(int);
void cleanup(void);
void sendMessage(int, int);
int detachAndRemoveTimer(int, sharedStruct*);
int detachAndRemoveArray(int, PCB*);
int detachAndRemoveResource(int, resource*);
void printHelpMessage(void);
void printShortHelpMessage(void);

struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {0,     0,            0,  0},
  {}
};

//PCB Array//
PCB *pcbArray;
resource *resourceArray;

//Char arrays for arg passing to children//
char *mArg;
char *nArg;
char *pArg;
char *rArg;
char *tArg;

volatile sig_atomic_t cleanupCalled = 0;

pid_t myPid, childPid;
int tValue = 20;
int vFlag = 0;
int checkDeadlockFlag = 0;
long long lastDeadlockCheck = 0;
int status;
int shmid;
int pcbShmid;
int resourceShmid;
int slaveQueueId;
int masterQueueId;
int nextProcessToSend = 1;
int processNumberBeingSpawned = -1;
long long timeToSpawn = 0;
long long idleTime = 0;
long long turnaroundTime = 0;
long long processWaitTime = 0;
long long totalProcessLifeTime = 0;
int totalProcessesSpawned = 0;
int messageReceived = 0;
int *requested;
int *available;

struct sharedStruct *myStruct;

//Constants for timing the program
const long long MAX_TIME = 20000000000;
const int MAX_FUTURE_SPAWN = 280000001;
const int MAX_IDLE_INCREMENT = 100000001;
const int MAX_TOTAL_PROCESS_TIME = 700000001;
const int CHANCE_HIGH_PRIORITY = 20;
const int MAXSLAVE = 20;
const int ARRAY_SIZE = 18;
FILE *file;
struct msqid_ds msqid_ds_buf;

key_t timerKey = 148364;
key_t pcbArrayKey = 135155;
key_t resourceKey = 131581;
key_t masterQueueKey = 128464;
#endif

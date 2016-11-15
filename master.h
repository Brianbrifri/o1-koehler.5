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
long long getProcessPriority(void);
int scheduleProcessTime(void);
void updateAverageTurnaroundTime(int);
pid_t scheduleNextProcess(void);
int waitForTurn(void);
void updateAfterProcessFinish(int);
void interruptHandler(int);
void cleanup(void);
void sendMessage(int, int);
int detachAndRemoveTimer(int, sharedStruct*);
int detachAndRemoveArray(int, PCB*);
void printHelpMessage(void);
void printShortHelpMessage(void);

struct option long_options[] = {
  {"help", no_argument, 0, 'h'},
  {0,     0,            0,  0},
  {}
};

//PCB Array//
struct PCB *pcbArray;

//Begin queue stuff//

struct queue {
  pid_t id;
  struct queue *next;

} *front0, *front1, *front2, *front3,
  *rear0, *rear1, *rear2, *rear3,
  *temp0, *temp1, *temp2, *temp3,
  *frontA0, *frontA1, *frontA2, *frontA3;

int queue0size;
int queue1size;
int queue2size;
int queue3size;

const long long queuePriorityHigh = 40000000;
const long long queuePriorityNormal_1 = 30000000;
const long long queuePriorityNormal_2 = 60000000;
const long long queuePriorityNormal_3 = 120000000;

void createQueues(void);
bool isEmpty(int);
void Enqueue(pid_t, int);
pid_t pop(int);
void clearQueues(void);

const int QUEUE0 = 0;
const int QUEUE1 = 1;
const int QUEUE2 = 2;
const int QUEUE3 = 3;

//End Queue Stuff//
//Char arrays for arg passing to children//
char *mArg;
char *nArg;
char *pArg;
char *tArg;

volatile sig_atomic_t cleanupCalled = 0;

pid_t myPid, childPid;
int tValue = 20;
int sValue = 3;
int status;
int shmid;
int pcbShmid;
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
//long long *ossTimer = 0;

struct sharedStruct *myStruct;

//Constants for timing the program
const long long MAX_TIME = 20000000000;
const int MAX_FUTURE_SPAWN = 280000001;
const int MAX_IDLE_INCREMENT = 10001;
const int MAX_TOTAL_PROCESS_TIME = 700000001;
const int CHANCE_HIGH_PRIORITY = 20;
const int MAXSLAVE = 20;
const int ARRAY_SIZE = 18;
FILE *file;
struct msqid_ds msqid_ds_buf;

key_t timerKey = 148364;
key_t pcbArrayKey = 135155;
key_t masterQueueKey = 128464;
#endif

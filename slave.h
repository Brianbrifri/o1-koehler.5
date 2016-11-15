#ifndef SLAVE_H
#define SLAVE_H
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
#include <signal.h>
#include <errno.h>
#include <time.h>
int willBlockIO(void);
long long getPartialQuantum(void);
void sendMessage(int, int);
void getMessage(int, int);
void alarmHandler(int);
void sigquitHandler(int);
void zombieKiller(int);
volatile sig_atomic_t sigNotReceived = 1;
pid_t myPid;
long long *ossTimer;
struct sharedStruct *myStruct;
int processNumber = 0;
int masterQueueId;
const int QUIT_TIMEOUT = 10;
struct msqid_ds msqid_buf;
struct PCB *pcbArray;
#endif

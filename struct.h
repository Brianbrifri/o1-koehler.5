#ifndef STRUCT_H
#define STRUCT_H
#include <sys/types.h>
#include <stdbool.h>
#define BLK "\x1b[30m"
#define RED "\x1b[31m"
#define GRN "\x1b[32m"
#define YLW "\x1b[33m"
#define BLU "\x1b[34m"
#define MAG "\x1b[35m"
#define CYAN "\x1b[36m"
#define LTBLU "\x1b[94m"
#define NRM "\x1b[0m"
#define BLKBK "\x1b[40m"
#define REDBK "\x1b[41m"
#define GRNBK "\x1b[42m"
#define YLWBK "\x1b[43m"
#define BLUBK "\x1b[44m"
#define MAGBK "\x1b[45m"
#define CYANBK "\x1b[46m"
#define BOLD "\x1b[1m"
#define DIM "\x1b[2m"
#define UNDL "\x1b[4m"
#define INVT "\x1b[7m"
#define BBU "\x1b[34;1;4m"
#define RBU "\x1b[31;1;4m"
#define YBU "\x1b[33;1;4m"
#define MBU "\x1b[35;1;4m"
#define Q0 "\x1b[38;5;226m"
#define Q1 "\x1b[38;5;216m"
#define Q2 "\x1b[38;5;206m"
#define Q3 "\x1b[38;5;166m"
#define TIMER "\x1b[38;5;45m"
#define IDLE "\x1b[38;5;100m"
#define LTBLUDIM "\x1b[94;2m"
#define REPORT "\x1b[48;5;39m"
static const long long NANO_MODIFIER = 1000000000;

typedef struct sharedStruct {
  long long ossTimer;
  int sigNotReceived;
  pid_t scheduledProcess;
} sharedStruct;

typedef struct msgbuf {
  long mType;
  char mText[80];
} msgbuf;

typedef struct PCB {
  pid_t processID;
  long long totalScheduledTime;
  long long totalTimeRan;
  long long lastBurst;
  long long priority;
  long long createTime;
} PCB;

#endif

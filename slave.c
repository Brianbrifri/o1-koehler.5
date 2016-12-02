
#include "slave.h"

int main (int argc, char **argv) {
  int timeoutValue = 30;
  long long startTime;
  long long currentTime;
  key_t masterKey = 128464;
  key_t slaveKey = 120314;

  int shmid = 0;
  int pcbShmid = 0;
  int resourceShmid = 0;

  char *fileName;
  char *defaultFileName = "test.out";
  char *option = NULL;
  char *short_options = "l:m:n:p:r:t:";
  FILE *file;
  int c;
  myPid = getpid();

  //get options from parent process
  opterr = 0;
  while((c = getopt(argc, argv, short_options)) != -1)
    switch (c) {
      case 'l':
        fileName = optarg;
        break;
      case 'm':
        shmid = atoi(optarg);
        break;
      case 'n':
        processNumber = atoi(optarg);
        break;
      case 'p':
        pcbShmid = atoi(optarg);
        break;
      case 'r':
        resourceShmid = atoi(optarg);
        break;
      case 't':
        timeoutValue = atoi(optarg) + 2;
        break;
      case '?':
        fprintf(stderr, "    Arguments were not passed correctly to slave %d. Terminating.\n", myPid);
        exit(-1);
    }

  srand(time(NULL) + processNumber);

  //Try to attach to shared memory
  if((myStruct = (sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach shared mem");
    exit(1);
  }

  if((pcbArray = (PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach to shared memory array");
    exit(1);
  }

  if((resourceArray = (resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
    perror("    Slave could not attach to resource shared mem seg");
    exit(1);
  }

  //Ignore SIGINT so that it can be handled below
  signal(SIGINT, SIG_IGN);

  //Set the sigquitHandler for the SIGQUIT signal
  signal(SIGQUIT, sigquitHandler);

  //Set the alarm handler
  signal(SIGALRM, zombieKiller);

  //Set the default alarm time
  alarm(QUIT_TIMEOUT);

  if((masterQueueId = msgget(masterKey, IPC_CREAT | 0777)) == -1) {
    perror("    Slave msgget for master queue");
    exit(-1);
  }

  //Set an alarm to 10 more seconds than the parent process
  //so that the child will be killed if parents gets killed
  //and child becomes slave of init
  alarm(timeoutValue);

  int i = 0;
  int j;

  long long duration;
  int notFinished = 1;

  do {
  
    //If this process' request flag is -1, it is not waiting on anything
    //go ahead and determine an action
    if(pcbArray[processNumber].request == -1 && pcbArray[processNumber].release == -1) {
      //printf("    Slave %d has no request\n", processNumber);
      //Check to see if process will terminate
      if(willTerminate()) {
        notFinished = 0;
      }
      //if not, see what other action it will take
      else {
        if(takeAction()) {
          int choice = rand() % 2;
          //Request a resource
          if(choice) {
            pcbArray[processNumber].request = chooseResource(); 
           sendMessage(masterQueueId, 3);
          }
          //Release a resource
          else {
            int i;
            for(i = 0; i < 20; i++) {
              if(pcbArray[processNumber].allocation.quantity[i] > 0) {
                printf("    Slave %d releasing %d\n", processNumber, i);
                pcbArray[processNumber].release = i;
                break;
              }
            }
            sendMessage(masterQueueId, 3);
          }
        }
      }
    }
  } while (notFinished && myStruct->sigNotReceived);

  pcbArray[processNumber].processID = -1;
  sendMessage(masterQueueId, 3);

  if(shmdt(myStruct) == -1) {
    perror("    Slave could not detach shared memory struct");
  }

  if(shmdt(pcbArray) == -1) {
    perror("    Slave could not detach from shared memory array");
  }

  if(shmdt(resourceArray) == -1) {
    perror("    Slave could not detach from resource array");
  }

  printf("    %sSlave%s %s%d%s%s exiting%s\n", RED, NRM, RBU, processNumber, NRM, RED, NRM);
  //exit(1);
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
  printf("    Slave error\n");

}

int willTerminate(void) {
  if(myStruct->ossTimer - pcbArray[processNumber].createTime >= NANO_MODIFIER) {
    int choice = 1 + rand() % 5;
    return choice == 1 ? 1 : 0;
  }
  return 0;
}

int chooseResource(void) {
  int choice = rand() % 20;
  return choice;
}

int takeAction(void) {
  int choice = rand() % 2;
  return choice;
}

void sendMessage(int qid, int msgtype) {
  struct msgbuf msg;

  msg.mType = msgtype;
  sprintf(msg.mText, "%d", processNumber);

  if(msgsnd(qid, (void *) &msg, sizeof(msg.mText), IPC_NOWAIT) == -1) {
    perror("    Slave msgsnd error");
  }
}

//This handles SIGQUIT being sent from parent process
//It sets the volatile int to 0 so that it will not enter in the CS.
void sigquitHandler(int sig) {
  printf("    Slave %s%d%s has received signal %s (%d)\n", RBU, processNumber, NRM, strsignal(sig), sig);

  if(shmdt(myStruct) == -1) {
    perror("    Slave could not detach shared memory");
  }

  if(shmdt(pcbArray) == -1) {
    perror("    Slave could not detach from shared memory array");
  }

  if(shmdt(resourceArray) == -1) {
    perror("    Slave could not detach from resource array");
  }

  kill(myPid, SIGKILL);

  //The slaves have at most 5 more seconds to exit gracefully or they will be SIGTERM'd
  alarm(5);
}

//function to kill itself if the alarm goes off,
//signaling that the parent could not kill it off
void zombieKiller(int sig) {
  printf("    %sSlave %d is killing itself due to slave timeout override%s\n", MAG, processNumber, NRM);
  kill(myPid, SIGTERM);
  sleep(1);
  kill(myPid, SIGKILL);
}

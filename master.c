#include "master.h"

int main (int argc, char **argv)
{
  srand(time(0));
  mArg = malloc(20);
  nArg = malloc(20);
  pArg = malloc(20);
  tArg = malloc(20);
  int hflag = 0;
  int nonOptArgFlag = 0;
  int index;
  char *filename = "test.out";
  char *defaultFileName = "test.out";
  char *programName = argv[0];
  char *option = NULL;
  char *short_options = "hs:l:t:";
  int c;

  
  //process arguments
  opterr = 0;
  while ((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1)
    switch (c) {
      case 'h':
        hflag = 1;
        break;
      case 's':
        sValue = atoi(optarg);
        if(sValue > MAXSLAVE) {
          sValue = 20;
          fprintf(stderr, "No more than 20 slave processes allowed at a time. Reverting to 20.\n");
        }
        break;
      case 'l':
        filename = optarg;
        break;
      case 't':
        tValue = atoi(optarg);
        break;
      case '?':
        if (optopt == 's') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          sValue = 5;
        }
        else if (optopt == 'l') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          filename = defaultFileName;
        }
        else if (optopt == 't') {
          fprintf(stderr, "Option -%c requires an argument. Using default value.\n", optopt);
          tValue = 20;
        }
        else if (isprint (optopt)) {
          fprintf(stderr, "Unknown option -%c. Terminating.\n", optopt);
          return -1;
        }
        else {
          printShortHelpMessage();
          return 0;
        }
      }


  //print out all non-option arguments
  for (index = optind; index < argc; index++) {
    fprintf(stderr, "Non-option argument %s\n", argv[index]);
    nonOptArgFlag = 1;
  }

  //if above printed out, print short help message
  //and return from process
  if(nonOptArgFlag) {
    printShortHelpMessage();
    return 0;
  }

  //if help flag was activated, print help message
  //then return from process
  if(hflag) {
    printHelpMessage();
    return 0;
  }

  //****START PROCESS MANAGEMENT****//

  //Initialize the alarm and CTRL-C handler
  signal(SIGALRM, interruptHandler);
  signal(SIGINT, interruptHandler);
  signal(SIGCHLD, SIG_IGN);
  signal(SIGQUIT, SIG_IGN);

  //set the alarm to tValue seconds
  alarm(tValue);

  int sizeArray = sizeof(*pcbArray) * 18;

  //Try to get the shared mem id from the key with a size of the struct
  //create it with all perms
  if((shmid = shmget(timerKey, sizeof(sharedStruct), IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation shared struct");
    exit(-1);
  }

  //Try to attach the struct pointer to shared memory
  if((myStruct = (struct sharedStruct *)shmat(shmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach shared mem");
    exit(-1);
  }
  
  //get shmid for pcbArray of 18 pcbs
  if((pcbShmid = shmget(pcbArrayKey, sizeArray, IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation pcb array");
    exit(-1);
  }

  //try to attach pcb array to shared memory
  if((pcbArray = (struct PCB *)shmat(pcbShmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach to pcb array");
    exit(-1);
  }

  //create message queue for the master process
  if((masterQueueId = msgget(masterQueueKey, IPC_CREAT | 0777)) == -1) {
    perror("Master msgget for master queue");
    exit(-1);
  }


  int i;
  for (i = 0; i < ARRAY_SIZE; i++) {
    pcbArray[i].processID = 0;
    pcbArray[i].priority = 0;
    pcbArray[i].totalScheduledTime = 0;
    pcbArray[i].lastBurst = 0;
    pcbArray[i].totalTimeRan = 0;
    pcbArray[i].createTime = 0;
  }

  //Open file and mark the beginning of the new log
  file = fopen(filename, "w");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }

  myStruct->ossTimer = 0;
  myStruct->scheduledProcess = -1;
  myStruct->sigNotReceived = 1;

  createQueues();

  fprintf(file,"***** BEGIN LOG *****\n");

  int tempIncrement;
  do {

    if(isTimeToSpawn()) {
      spawnSlave();
      setTimeToSpawn();
    }

    myStruct->scheduledProcess = scheduleNextProcess();

    tempIncrement = incrementTimer();
    idleTime += tempIncrement;
    myStruct->ossTimer += tempIncrement;
    printf("until %s%llu.%09llu%s\n", TIMER, myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER, NRM);

    updateAfterProcessFinish(waitForTurn());
  
  } while (myStruct->ossTimer < MAX_TIME && myStruct->sigNotReceived);

  if(!cleanupCalled) {
    cleanupCalled = 1;
    cleanup();
  }
  return 0;
}

bool isTimeToSpawn(void) {
  printf("%sChecking time to spawn: future = %llu.%09llu timer = %llu.%09llu%s\n", DIM, timeToSpawn / NANO_MODIFIER, timeToSpawn % NANO_MODIFIER, myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER, NRM);
  fprintf(file, "Checking time to spawn: future = %llu timer = %llu\n", timeToSpawn, myStruct->ossTimer);
  return myStruct->ossTimer >= timeToSpawn ? true : false;
}

void setTimeToSpawn(void) {
  timeToSpawn = myStruct->ossTimer + rand() % MAX_FUTURE_SPAWN;
  printf("Will try to spawn slave at time %s%s%llu.%09llu%s\n", BLK, YLWBK, timeToSpawn / NANO_MODIFIER, timeToSpawn % NANO_MODIFIER, NRM);
  fprintf(file, "Will try to spawn slave at time %llu\n", timeToSpawn);
}

void spawnSlave(void) {

    processNumberBeingSpawned = -1;
    
    int i;
    for(i = 0; i < ARRAY_SIZE; i++) {
      if(pcbArray[i].processID == 0) {
        processNumberBeingSpawned = i;
        pcbArray[i].processID = 1;
        break;
      } 
    }

    if(processNumberBeingSpawned == -1) {
      printf("%sPCB array is full. No process created.%s\n", REDBK, NRM);
      fprintf(file, "PCB array is full. No process created.\n");
    }

    if(processNumberBeingSpawned != -1) {
      printf("%sFound open PCB. Spawning process.%s\n", GRNBK, NRM);
      totalProcessesSpawned = totalProcessesSpawned + 1;
      //exit on bad fork
      if((childPid = fork()) < 0) {
        perror("Fork Failure");
        //exit(1);
      }

      //If good fork, continue to call exec with all the necessary args
      if(childPid == 0) {
        printf("Total processes spawned: %d\n", totalProcessesSpawned);
        pcbArray[processNumberBeingSpawned].priority = getProcessPriority();
        pcbArray[processNumberBeingSpawned].totalScheduledTime = scheduleProcessTime();
        pcbArray[processNumberBeingSpawned].createTime = myStruct->ossTimer;
        pcbArray[processNumberBeingSpawned].processID = getpid();
        printf("Process %s%d%s at location %s%d%s was scheduled for duration %s%llu.%09llu%s at time %s%llu.%09llu%s\n", BBU, getpid(), NRM, RBU, processNumberBeingSpawned, NRM, YBU, pcbArray[processNumberBeingSpawned].totalScheduledTime / NANO_MODIFIER, pcbArray[processNumberBeingSpawned].totalScheduledTime % NANO_MODIFIER, NRM, TIMER, myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER, NRM);
        fprintf(file, "Process %d at location %d was scheduled for duration %llu\n", getpid(), processNumberBeingSpawned, pcbArray[processNumberBeingSpawned].totalScheduledTime);
        sprintf(mArg, "%d", shmid);
        sprintf(nArg, "%d", processNumberBeingSpawned);
        sprintf(pArg, "%d", pcbShmid);
        sprintf(tArg, "%d", tValue);
        char *slaveOptions[] = {"./slaverunner", "-m", mArg, "-n", nArg, "-p", pArg, "-t", tArg, (char *)0};
        execv("./slaverunner", slaveOptions);
        fprintf(stderr, "    Should only print this in error\n");
      }
      
    }
    if(processNumberBeingSpawned != -1) {
      while(pcbArray[processNumberBeingSpawned].processID <= 1); 
      if(pcbArray[processNumberBeingSpawned].priority == queuePriorityHigh) {
        Enqueue(pcbArray[processNumberBeingSpawned].processID, QUEUE0);
      }
      else {
        Enqueue(pcbArray[processNumberBeingSpawned].processID, QUEUE1);
      }
    }
}



int incrementTimer(void) {
  int random = 1 + rand() % MAX_IDLE_INCREMENT;
  printf("Spent %s%d%s in master ", IDLE, random, NRM);
  return random;
}

int scheduleProcessTime(void) {
  return 1 + rand() % MAX_TOTAL_PROCESS_TIME;
}

long long getProcessPriority(void) {
  int random = rand() % CHANCE_HIGH_PRIORITY;
  return random == 1 ? queuePriorityHigh : queuePriorityNormal_1;
}

int waitForTurn(void) {
  struct msgbuf msg;

  while(myStruct->scheduledProcess != -1);

  if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("Error master receiving message");
      return -1;
    }
    printf("No message for master\n");
    return -1;
  }
  else {
    int processNum = atoi(msg.mText);
    printf("Message from %s%d%s:%s%d%s at time %s%llu.%09llu%s\n", BBU, pcbArray[processNum].processID, NRM, RBU, processNum, NRM, TIMER, myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER, NRM);
    fprintf(file, "Message from %d:%d at time %llu\n", pcbArray[processNum].processID, processNum, myStruct->ossTimer);
    fprintf(file, "    Slave %d:%d got duration %llu out of %llu\n", pcbArray[processNum].processID, processNum, pcbArray[processNum].lastBurst, pcbArray[processNum].priority);
    fprintf(file, "    Slave %d:%d has ran for a total of %llu out of %llu\n", pcbArray[processNum].processID, processNum, pcbArray[processNum].totalTimeRan, pcbArray[processNum].totalScheduledTime);
    return processNum;
  }
}

void updateAverageTurnaroundTime(int pcb) {

  long long startToFinish = myStruct->ossTimer - pcbArray[pcb].createTime;
  totalProcessLifeTime += startToFinish;
  processWaitTime += startToFinish - pcbArray[pcb].totalScheduledTime;
}

void updateAfterProcessFinish(int processLocation) {

  if(processLocation == -1) {
    return;
  }

  pid_t id = pcbArray[processLocation].processID;
  long long lastBurst = pcbArray[processLocation].lastBurst;
  long long priority = pcbArray[processLocation].priority;

  if(id != 0) {
    if(priority == queuePriorityHigh) {
      Enqueue(id, QUEUE0);
      return;
    }
    else if(lastBurst < priority) {
      pcbArray[processLocation].priority = queuePriorityNormal_1;
      Enqueue(id, QUEUE1);
      return;
    }
    else {
      if(priority == queuePriorityNormal_1) {
        pcbArray[processLocation].priority = queuePriorityNormal_2;
        Enqueue(id, QUEUE2);
        return;
      }
      else if(priority == queuePriorityNormal_2) {
        pcbArray[processLocation].priority = queuePriorityNormal_3;
        Enqueue(id, QUEUE3);
        return;
      }
      else if(priority == queuePriorityNormal_3) {
        pcbArray[processLocation].priority = queuePriorityNormal_3;
        Enqueue(id, QUEUE3);
        return;
      }
      else {
        printf("Unhandled priority exception\n");
      
      }

    }
 
  }
  else {
    printf("%sProcess completed its time%s\n", GRN, NRM);
    fprintf(file, "Process completed its time\n");
    updateAverageTurnaroundTime(processLocation);
    pcbArray[processLocation].totalScheduledTime = 0;
    pcbArray[processLocation].lastBurst = 0;
    pcbArray[processLocation].totalTimeRan = 0;
    pcbArray[processLocation].createTime = 0;
  }

}

pid_t scheduleNextProcess(void) {
  if(!isEmpty(QUEUE0)) {
    return pop(QUEUE0);
  }
  else if(!isEmpty(QUEUE1)) {
    return pop(QUEUE1);
  }
  else if(!isEmpty(QUEUE2)) {
    return pop(QUEUE2);
  }
  else if(!isEmpty(QUEUE3)) {
    return pop(QUEUE3);
  }
  else return -1;
}

//Set queue pointers to null
void createQueues() {
  front0 = front1 = front2 = front3 = NULL;
  rear0 = rear1 = rear2 = rear3 = NULL;
  queue0size = queue1size = queue2size = queue3size = 0;
}

//Function to check if a queue is empty
bool isEmpty(int choice) {
  switch(choice) {
    case 0:
      if((front0 == NULL) && (rear0 == NULL))
        return true;
      break;
    case 1:
      if((front1 == NULL) && (rear1 == NULL))
        return true;
      break;
    case 2:
      if((front2 == NULL) && (rear2 == NULL))
        return true;
      break;
    case 3:
      if((front3 == NULL) && (rear3 == NULL))
        return true;
      break;
    default:
      printf("Not a valid queue choice\n");
  }
  return false;
}

//Function to add a process id to a given queue
void Enqueue(pid_t processId, int choice) {
  fprintf(file, "Enqueuing pid %d in queue %d\n", processId, choice);
  switch(choice) {
    case 0:
      printf("Enqueuing pid %s%d%s in queue %s%d%s\n", BBU, processId, NRM, Q0, choice, NRM);
      if(rear0 == NULL) {
        rear0 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear0->next = NULL;
        rear0->id = processId;
        front0 = rear0;
      }
      else {
        temp0 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear0->next = temp0;
        temp0->id = processId;
        temp0->next = NULL;

        rear0 = temp0;
      }
      queue0size++;
      break;
    case 1:
      printf("Enqueuing pid %s%d%s in queue %s%d%s\n", BBU, processId, NRM, Q1, choice, NRM);
      if(rear1 == NULL) {
        rear1 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear1->next = NULL;
        rear1->id = processId;
        front1 = rear1;
      }
      else {
        temp1 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear1->next = temp1;
        temp1->id = processId;
        temp1->next = NULL;

        rear1 = temp1;
      }
      queue1size++;
      break;
    case 2:
      printf("Enqueuing pid %s%d%s in queue %s%d%s\n", BBU, processId, NRM, Q2, choice, NRM);
      if(rear2 == NULL) {
        rear2 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear2->next = NULL;
        rear2->id = processId;
        front2 = rear2;
      }
      else {
        temp2 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear2->next = temp2;
        temp2->id = processId;
        temp2->next = NULL;

        rear2 = temp2;
      }
      queue2size++;
      break;
    case 3:
      printf("Enqueuing pid %s%d%s in queue %s%d%s\n", BBU, processId, NRM, Q3, choice, NRM);
      if(rear3 == NULL) {
        rear3 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear3->next = NULL;
        rear3->id = processId;
        front3 = rear3;
      }
      else {
        temp3 = (struct queue*)malloc(1 * sizeof(struct queue));
        rear3->next = temp3;
        temp3->id = processId;
        temp3->next = NULL;

        rear3 = temp3;
      }
      queue3size++;
      break;
    default:
      printf("Not a valid queue choice\n");
  }
}

//function to pop the process id for a given queue
pid_t pop(int choice) {
  pid_t poppedID;
  switch(choice) {
    case 0:
      frontA0 = front0;
      if(frontA0 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA0->next != NULL) {
          frontA0 = frontA0->next;
          poppedID = front0->id;
          free(front0);
          front0 = frontA0;
        }
        else {
          poppedID = front0->id;
          free(front0);
          front0 = NULL;
          rear0 = NULL;
        }
        printf("Got pid %s%d%s from queue %s%d%s\n", BBU, poppedID, NRM, Q0, choice, NRM);
        queue0size--;
      }
      break;
    case 1:
      frontA1 = front1;
      if(frontA1 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA1->next != NULL) {
          frontA1 = frontA1->next;
          poppedID = front1->id;
          free(front1);
          front1 = frontA1;
        }
        else {
          poppedID = front1->id;
          free(front1);
          front1 = NULL;
          rear1 = NULL;
        }
        printf("Got pid %s%d%s from queue %s%d%s\n", BBU, poppedID, NRM, Q1, choice, NRM);
        queue1size--;
      }
      break;
    case 2:
      frontA2 = front2;
      if(frontA2 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA2->next != NULL) {
          frontA2 = frontA2->next;
          poppedID = front2->id;
          free(front2);
          front2 = frontA2;
        }
        else {
          poppedID = front2->id;
          free(front2);
          front2 = NULL;
          rear2 = NULL;
        }
        printf("Got pid %s%d%s from queue %s%d%s\n", BBU, poppedID, NRM, Q2, choice, NRM);
        queue2size--;
      }
      break;
    case 3:
      frontA3 = front3;
      if(frontA3 == NULL) {
        printf("Error: popping an empty queue\n");
      }
      else {
        if(frontA3->next != NULL) {
          frontA3 = frontA3->next;
          poppedID = front3->id;
          free(front3);
          front3 = frontA3;
        }
        else {
          poppedID = front3->id;
          free(front3);
          front3 = NULL;
          rear3 = NULL;
        }
        printf("Got pid %s%d%s from queue %s%d%s\n", BBU, poppedID, NRM, Q3, choice, NRM);
        queue3size--;
      }
      break;
    default:
      printf("Not a valid queue choice\n");
  }
  fprintf(file, "Got pid %d from queue %d\n", poppedID, choice);
  return poppedID;
}

//Interrupt handler function that calls the process destroyer
//Ignore SIGQUIT and SIGINT signal, not SIGALRM, so that
//I can handle those two how I want
void interruptHandler(int SIG){
  signal(SIGQUIT, SIG_IGN);
  signal(SIGINT, SIG_IGN);

  if(SIG == SIGINT) {
    fprintf(stderr, "\n%sCTRL-C received. Calling shutdown functions.%s\n", RED, NRM);
  }

  if(SIG == SIGALRM) {
    fprintf(stderr, "%sMaster has timed out. Initiating shutdown sequence.%s\n", RED, NRM);
  }

  if(!cleanupCalled) {
    cleanupCalled = 1;
    cleanup();
  }
}

//Cleanup memory and processes.
//kill calls SIGQUIT on the groupid to kill the children but
//not the parent
void cleanup() {

  signal(SIGQUIT, SIG_IGN);
  myStruct->sigNotReceived = 0;

  printf("%sMaster sending SIGQUIT%s\n", RED, NRM);
  kill(-getpgrp(), SIGQUIT);

  //free up the malloc'd memory for the arguments
  free(mArg);
  free(nArg);
  free(pArg);
  free(tArg);
  printf("%sMaster waiting on all processes do die%s\n", RED, NRM);
  childPid = wait(&status);

  //Detach and remove the shared memory after all child process have died
  if(detachAndRemoveTimer(shmid, myStruct) == -1) {
    perror("Failed to destroy shared memory segment");
  }

  if(detachAndRemoveArray(pcbShmid, pcbArray) == -1) {
    perror("Failed to destroy shared memory segment");
  }

  clearQueues();

  printf("%sMaster about to delete message queues%s\n", RED, NRM);
  //Delete the message queues
  msgctl(slaveQueueId, IPC_RMID, NULL);
  msgctl(masterQueueId, IPC_RMID, NULL);

  if(fclose(file)) {
    perror("    Error closing file");
  }
  printf("%sMaster about to terminate%s\n", RED, NRM);

  printf("\n\n\n\n");
  printf("%s%s******************REPORT*****************%s\n", RED, REPORT, NRM);
  printf("%s%s*                                       *%s\n", RED, REPORT, NRM);
  printf("%s%s*             CPU IDLE TIME:            *%s\n", RED, REPORT, NRM);
  printf("%s%s*             %llu.%09llu               *%s\n", RED, REPORT, idleTime / NANO_MODIFIER, idleTime % NANO_MODIFIER, NRM);
  printf("%s%s*                                       *%s\n", RED, REPORT, NRM);
  printf("%s%s*            AVG TURNAROUND:            *%s\n", RED, REPORT, NRM);
  printf("%s%s*             %llu.%09llu               *%s\n", RED, REPORT, (totalProcessLifeTime / totalProcessesSpawned) / NANO_MODIFIER, (totalProcessLifeTime / totalProcessesSpawned) % NANO_MODIFIER, NRM);
  printf("%s%s*                                       *%s\n", RED, REPORT, NRM);
  printf("%s%s*               AVG WAIT:               *%s\n", RED, REPORT, NRM);
  printf("%s%s*             %llu.%09llu               *%s\n", RED, REPORT, (processWaitTime / totalProcessesSpawned) / NANO_MODIFIER, (processWaitTime / totalProcessesSpawned) % NANO_MODIFIER, NRM);
  printf("%s%s*                                       *%s\n", RED, REPORT, NRM);
  printf("%s%s*****************************************%s\n", RED, REPORT, NRM);

  printf("\n\n\n\n");
 
  exit(1);
  //Kill this master process
  //kill(getpid(), SIGKILL);
}


//make sure all the nodes in the queues are freed
void clearQueues(void) {
  while(!isEmpty(QUEUE0)) {
    pop(QUEUE0);
  }
  while(!isEmpty(QUEUE1)) {
    pop(QUEUE1);
  }
  while(!isEmpty(QUEUE2)) {
    pop(QUEUE2);
  }
  while(!isEmpty(QUEUE3)) {
    pop(QUEUE3);
  }
}

//Detach and remove function
int detachAndRemoveTimer(int shmid, sharedStruct *shmaddr) {
  printf("%sMaster: Detach and Remove Shared Memory%s\n", RED, NRM);
  int error = 0;
  if(shmdt(shmaddr) == -1) {
    error = errno;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    error = errno;
  }
  if(!error) {
    return 0;
  }

  return -1;
}

//Detach and remove function
int detachAndRemoveArray(int shmid, PCB *shmaddr) {
  printf("%sMaster: Detach and Remove Shared Memory%s\n", RED, NRM);
  int error = 0;
  if(shmdt(shmaddr) == -1) {
    error = errno;
  }
  if((shmctl(shmid, IPC_RMID, NULL) == -1) && !error) {
    error = errno;
  }
  if(!error) {
    return 0;
  }

  return -1;
}

//Long help message
void printHelpMessage(void) {
    printf("\nThank you for using the help menu!\n");
    printf("The following is a helpful guide to enable you to use this\n");
    printf("slavedriver program to the best of your ability!\n\n");
    printf("-h, --help: Prints this help message.\n");
    printf("-s: Allows you to set the number of slave process waiting to run.\n");
    printf("\tThe default value is 5. The max is 20.\n");
    printf("-l: Allows you to set the filename for the logger so the aliens can see how bad you mess up.\n");
    printf("\tThe default value is test.out.\n");
    printf("-t: Allows you set the wait time for the master process until it kills the slaves.\n");
    printf("\tThe default value is 20.\n");
}

//short help message
void printShortHelpMessage(void) {
  printf("\nAcceptable options are:\n");
  printf("[-h], [--help], [-l][required_arg], [-s][required_arg], [-t][required_arg]\n\n");
}

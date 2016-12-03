#include "master.h"

int main (int argc, char **argv)
{
  srand(time(0));
  mArg = malloc(20);
  nArg = malloc(20);
  pArg = malloc(20);
  rArg = malloc(20);
  tArg = malloc(20);
  int hflag = 0;
  int nonOptArgFlag = 0;
  int index;
  char *filename = "test.out";
  char *defaultFileName = "test.out";
  char *programName = argv[0];
  char *option = NULL;
  char *short_options = "hl:t:v";
  int c;

  
  //process arguments
  opterr = 0;
  while ((c = getopt_long (argc, argv, short_options, long_options, NULL)) != -1)
    switch (c) {
      case 'h':
        hflag = 1;
        break;
      case 'v':
        vFlag = 1;
        break;
      case 'l':
        filename = optarg;
        break;
      case 't':
        tValue = atoi(optarg);
        break;
      case '?':
        if (optopt == 'l') {
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
  int sizeResource = sizeof(*resourceArray) * 20;

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

  if((resourceShmid = shmget(resourceKey, sizeResource, IPC_CREAT | 0777)) == -1) {
    perror("Bad shmget allocation resource array");
    exit(-1);
  }

  if((resourceArray = (struct resource *)shmat(resourceShmid, NULL, 0)) == (void *) -1) {
    perror("Master could not attach to resource array");
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
    pcbArray[i].terminate = 0;
    pcbArray[i].request = -1;
    pcbArray[i].release = -1;
    pcbArray[i].totalTimeRan = 0;
    pcbArray[i].createTime = 0;
    int j;
    for(j = 0; j < 20; j++) {
      pcbArray[i].allocation.quantity[j] = 0;
    }
  }

  setupResources();

  //Open file and mark the beginning of the new log
  file = fopen(filename, "w");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }

  myStruct->ossTimer = 0;
  myStruct->sigNotReceived = 1;

  fprintf(file,"***** BEGIN LOG *****\n");

  do {

    if(isTimeToSpawn()) {
      spawnSlave();
      setTimeToSpawn();
    }
    if(myStruct->ossTimer - lastDeadlockCheck > NANO_MODIFIER) {
      lastDeadlockCheck = myStruct->ossTimer;
      printf("%s---------------------------BEGIN DEADLOCK STUFF--------------------%s\n", TIMER, NRM);
      if(deadlock()) {
        do {
          killAProcess();
        }while(deadlock());
      }
      printf("%s---------------------------END DEADLOCK STUFF--------------------%s\n", TIMER, NRM);
    }

    myStruct->ossTimer += incrementTimer();

    
    if(vFlag) {
      printf("---------------------------BEGIN MESSAGECHECKING--------------------\n");
    }
    performActionsFromMessage(processMessageQueue());
    if(vFlag) {
       printf("----------------------------END MESSAGECHECKING---------------------\n");
    }
    checkAndProcessRequests();

    if(!vFlag) {
      sleep(.01);
    }   
  
  } while (myStruct->ossTimer < MAX_TIME && myStruct->sigNotReceived);

  if(!cleanupCalled) {
    cleanupCalled = 1;
    cleanup();
  }
  return 0;
}

bool isTimeToSpawn(void) {
  if(vFlag) {
    printf("%sChecking time to spawn: future = %llu.%09llu timer = %llu.%09llu%s\n", DIM, timeToSpawn / NANO_MODIFIER, timeToSpawn % NANO_MODIFIER, myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER, NRM);
  }
  if(vFlag) {
    fprintf(file, "Checking time to spawn: future = %llu timer = %llu\n", timeToSpawn, myStruct->ossTimer);
  }
  return myStruct->ossTimer >= timeToSpawn ? true : false;
}

void setTimeToSpawn(void) {
  timeToSpawn = myStruct->ossTimer + rand() % MAX_FUTURE_SPAWN;
  if(vFlag) {
    printf("Will try to spawn slave at time %s%s%llu.%09llu%s\n", BLK, YLWBK, timeToSpawn / NANO_MODIFIER, timeToSpawn % NANO_MODIFIER, NRM);
  }        
  if(vFlag) {
    fprintf(file, "Will try to spawn slave at time %llu\n", timeToSpawn);
  }
}

void spawnSlave(void) {

    resourceSnapshot();
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
      if(vFlag) {
        printf("%sPCB array is full. No process created.%s\n", REDBK, NRM);
      }
      if(vFlag) {
        fprintf(file, "PCB array is full. No process created.\n");
      }
    }

    if(processNumberBeingSpawned != -1) {
      if(vFlag) {
        printf("%sFound open PCB. Spawning process.%s\n", GRNBK, NRM);
      }
      totalProcessesSpawned = totalProcessesSpawned + 1;
      //exit on bad fork
      if((childPid = fork()) < 0) {
        perror("Fork Failure");
        //exit(1);
      }

      //If good fork, continue to call exec with all the necessary args
      if(childPid == 0) {
        if(vFlag) {
          printf("Total processes spawned: %d\n", totalProcessesSpawned);
        }
        pcbArray[processNumberBeingSpawned].createTime = myStruct->ossTimer;
        pcbArray[processNumberBeingSpawned].processID = getpid();
        sprintf(mArg, "%d", shmid);
        sprintf(nArg, "%d", processNumberBeingSpawned);
        sprintf(pArg, "%d", pcbShmid);
        sprintf(rArg, "%d", resourceShmid);
        sprintf(tArg, "%d", tValue);
        char *slaveOptions[] = {"./slaverunner", "-m", mArg, "-n", nArg, "-p", pArg, "-r", rArg, "-t", tArg, (char *)0};
        execv("./slaverunner", slaveOptions);
        fprintf(stderr, "    Should only print this in error\n");
      }
      
    }
}



int incrementTimer(void) {
  int random = 1 + rand() % MAX_IDLE_INCREMENT;
  return random;
}

//Checks message queue and returns the process location of the sender in the array
int processMessageQueue(void) {
  struct msgbuf msg;

  if(msgrcv(masterQueueId, (void *) &msg, sizeof(msg.mText), 3, IPC_NOWAIT) == -1) {
    if(errno != ENOMSG) {
      perror("Error master receivine message");
      return -1;
    }
    if(vFlag) {
      printf("No message for master\n");
    }
    return -1;
  }
  else {
    int processNum = atoi(msg.mText);
    return processNum;
  }
}

void performActionsFromMessage(int processNum) {
  if(processNum == -1) {
    return;
  }
  int resourceType;
  if((resourceType = pcbArray[processNum].request) >= 0) {
    if(vFlag) {
      printf("Found a request from process %d for %d\n", processNum, resourceType);
    }
    //If there are resources of the type available, assign it
    performResourceRequest(resourceType, processNum);
  }
  else if ((resourceType = pcbArray[processNum].release) >= 0) {
    performResourceRelease(resourceType, processNum);
  }
  else if(pcbArray[processNum].processID == -1) {
    performProcessCleanup(processNum);
  }
  else {
    if(vFlag) {
      printf("Found no action for this message\n");
    }
  }

  performActionsFromMessage(processMessageQueue());

}

void updateAverageTurnaroundTime(int pcb) {
  long long startToFinish = myStruct->ossTimer - pcbArray[pcb].createTime;
  totalProcessLifeTime += startToFinish;
}


void setupResources(void) {
  int i;
  //Set the resource types, quantity, and quantAvail
  for(i = 0; i < 20; i++) {
    resourceArray[i].quantity = 1 + rand() % 10;
    resourceArray[i].quantAvail = resourceArray[i].quantity;
  }

  //Between 3 and 5 resources will be shareable
  int numShared = 3 + rand() % 3;

  //Get randomly choose a resource for those that
  //will be shared
  for(i = 0; i < numShared; i++) {
    int choice = rand() % 20;
    resourceArray[choice].quantity = 9999;
    resourceArray[choice].quantAvail = 9999;
  }

  resourceSnapshot();
}

void resourceSnapshot(void) {
  int i;
  for(i = 0; i < 20; i++) {
    if(vFlag) {
      printf("Resource %d has %d available out of %d\n", i, resourceArray[i].quantAvail, resourceArray[i].quantity);
    }
  }
}

void checkAndProcessRequests(void) {
  if(vFlag) {
    printf("---------------------------BEGIN CHECKREQUESTS--------------------\n");
  }
  int i;
  int j;
  int request = -1;
  int release = -1;
  //Go through and look at all the request/release/processID members of each pcbArray element
  //and see if there is any processing to do 
  for(i = 0; i < ARRAY_SIZE; i++) {
    int resourceType = -1;
    int quant;
    //If the request flag is set with the value of a resource type, process the request
    if((resourceType = pcbArray[i].request) >= 0) {
      if(vFlag) {
        printf("Found a request from process %d for %d\n", i, resourceType);
      }
      //If there are resources of the type available, assign it
      performResourceRequest(resourceType, i);
    }
    //If the release flag is set with the value of the resourceType, process it
    else if((resourceType = pcbArray[i].release) >= 0) {
      performResourceRelease(resourceType, i);
    }
    //If the process set its processID to -1, that means it died and we can put all
    //the resources it had back into the resourceArray
    else if(pcbArray[i].processID == -1){
      performProcessCleanup(i);    
    }
    else {
      //If there is a process at that location but doesn't meet the above criteria, print
      if(pcbArray[i].processID > 0) {
      }
    }
  }
  if(vFlag) {
    printf("---------------------------END CHECKREQUESTS--------------------\n");
  }
}

void performResourceRequest(int resourceType, int i) {
  int quant;
  if((quant = resourceArray[resourceType].quantAvail) > 0) {
    if(vFlag) {
      printf("There are %d out of %d for resource %d available\n", quant, resourceArray[resourceType].quantity, resourceType);
    }
    if(vFlag) {
      printf("Increased resource %d for process %d\n", resourceType, i);
    }
    //Increase the quantity of the resourceType for the element in the pcbArray
    //requesting it
    pcbArray[i].allocation.quantity[resourceType]++;
    //Reset the request to -1
    pcbArray[i].request = -1;
    //Decrease the quantity of the resource type in the resource array
    resourceArray[resourceType].quantAvail--;
    if(vFlag) {
      printf("There are now %d out of %d for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
    }
  }
}

void performResourceRelease(int resourceType, int i) {
  if(vFlag) {
    printf("Releasing resouce %d from process %d\n", resourceType, i);
  }
  //Decrease the count of the quantity of that resource for that element in the pcbArray
  pcbArray[i].allocation.quantity[resourceType]--;
  //Increase the quantity of that resource type in the resourceArray
  resourceArray[resourceType].quantAvail++;
  if(vFlag) {
    printf("There are now %d out of %d for resource %d\n", resourceArray[resourceType].quantAvail, resourceArray[resourceType].quantity, resourceType);
  }
  //Reset the release flag to -1
  pcbArray[i].release = -1;
}

void performProcessCleanup(int i) {
  if(vFlag) {
    printf("%sProcess %d completed its time%s\n", GRN, i, NRM);
  }
  if(vFlag) {
    fprintf(file, "Process completed its time\n");
  }
  //Go through all the allocations to the dead process and put them back into the
  //resource array
  int j;
  for(j = 0; j < 20; j++) {
    //If the quantity is > 0 for that resource, put them back
    if(pcbArray[i].allocation.quantity[j] > 0) {
      if(vFlag) {
        printf("Before return of resources, there are %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
      }
      //Get the quantity to put back
      int returnQuant = pcbArray[i].allocation.quantity[j];
      //Increase the resource type quantAvail in the resource array
      resourceArray[j].quantAvail += returnQuant;
      if(vFlag) {
        printf("Returning %d of resource %d from process %d\n", returnQuant, j, i);
      }
      if(vFlag) {
        printf("There are now %d out of %d for resource %d\n", resourceArray[j].quantAvail, resourceArray[j].quantity, j);
      }
      //Set the quantity of the pcbArray to 0
      pcbArray[i].allocation.quantity[j] = 0;
    } 
  }
  //Reset all values
  pcbArray[i].processID = 0;
  pcbArray[i].totalTimeRan = 0;
  pcbArray[i].createTime = 0;
  pcbArray[i].request = -1;
  pcbArray[i].release = -1;

}

int deadlock(void) {
  printf("Begin deadlock detection at %llu.%llu\n", myStruct->ossTimer / NANO_MODIFIER, myStruct->ossTimer % NANO_MODIFIER);
  int work[20];
  int finish[18];

  int p;
  for(p = 0; p < 20; p++) {
    work[p] = resourceArray[p].quantAvail;
  } 
  for(p = 0; p < 18; p++) {
    finish[p] = 0; 
  }
  for(p = 0; p < 18; p++) {
    if(!pcbArray[p].processID) {
      finish[p] = 1;
    }
    if(finish[p]) continue;
    if(reqLtAvail(work, p)) {
      finish[p] = 1;
      int i;
      for(i = 0; i < 20; i++) {
        work[i] += pcbArray[p].allocation.quantity[i];
      }
      p = -1;
    }       
  }


  int deadlockCount = 0;
  for(p = 0; p < 18; p++) {
    if(!finish[p]) {
      deadlockCount++; 
    }
  }

  if(deadlockCount > 0) {
    printf("%d processes: ", deadlockCount);
    for(p = 0; p < 18; p++) {
      if(!finish[p]) {
        printf("%d ", p);
      }
    }
    printf("are deadlocked\n");
    return deadlockCount;
  }
  else {
    printf("There are no deadlocks\n");
    return deadlockCount;
  }
}

int reqLtAvail(int *work, int p) {
  if(pcbArray[p].request == -1) {
    return 1;
  } 
  if(work[pcbArray[p].request] > 0) {
    return 1;
  }
  else {
    return 0;
  }
}

void killAProcess(void) {
  int process;
  int max = 0;
  int i;
  int j;
  for(i = 0; i < 18; i++) {
    int total = 0;
    for(j = 0; j < 20; j++) {
      total += pcbArray[i].allocation.quantity[j];
    } 
    if(total > max) {
      max = total;
      process = i;
    } 
  }
  printf("Killing process %d\n", process);
  pcbArray[process].terminate = 1;
  sleep(.01);
  performProcessCleanup(process);
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
  free(rArg);
  free(tArg);
   printf("%sMaster waiting on all processes do die%s\n", RED, NRM);
  childPid = wait(&status);

  //Detach and remove the shared memory after all child process have died
  if(detachAndRemoveTimer(shmid, myStruct) == -1) {
    perror("Failed to destroy shared messageQ shared mem seg");
  }

  if(detachAndRemoveArray(pcbShmid, pcbArray) == -1) {
    perror("Failed to destroy shared pcb shared mem seg");
  }

  if(detachAndRemoveResource(resourceShmid, resourceArray) == -1) {
    perror("Faild to destroy resource shared mem seg");
  }

   printf("%sMaster about to delete message queues%s\n", RED, NRM);
  //Delete the message queues
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

//Detach and remove function
int detachAndRemoveResource(int shmid, resource *shmaddr) {
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
    printf("-v: Allows you to set the vFlag flag to see all debug output.\n");
    printf("\tThe default value is 5. The max is 20.\n");
    printf("-l: Allows you to set the filename for the logger so the aliens can see how bad you mess up.\n");
    printf("\tThe default value is test.out.\n");
    printf("-t: Allows you set the wait time for the master process until it kills the slaves.\n");
    printf("\tThe default value is 20.\n");
}

//short help message
void printShortHelpMessage(void) {
  printf("\nAcceptable options are:\n");
  printf("[-h], [--help], [-l][required_arg], [-t][required_arg], [-v]\n\n");
}

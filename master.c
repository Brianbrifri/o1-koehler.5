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
    pcbArray[i].request = -1;
    pcbArray[i].release = -1;
    pcbArray[i].totalTimeRan = 0;
    pcbArray[i].createTime = 0;
    int j;
    for(j = 0; j < 20; j++) {
      pcbArray[i].allocation.type[j] = -1;
      pcbArray[i].allocation.quantity[j] = 0;
    }
  }

  //Open file and mark the beginning of the new log
  file = fopen(filename, "w");
  if(!file) {
    perror("Error opening file");
    exit(-1);
  }

  myStruct->ossTimer = 0;
  myStruct->sigNotReceived = 1;

  fprintf(file,"***** BEGIN LOG *****\n");

  int tempIncrement;
  do {

    if(isTimeToSpawn()) {
      spawnSlave();
      setTimeToSpawn();
    }

    myStruct->ossTimer += incrementTimer();

    updateAfterProcessFinish(processSystem());
  
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

int processSystem(void) {
  struct msgbuf msg;

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
    return processNum;
  }
}

void updateAverageTurnaroundTime(int pcb) {

  long long startToFinish = myStruct->ossTimer - pcbArray[pcb].createTime;
  totalProcessLifeTime += startToFinish;
}

void updateAfterProcessFinish(int processLocation) {

  //If no message received, no process performed any actions this check. Return.
  if(processLocation == -1) {
    return;
  }

  pid_t id = pcbArray[processLocation].processID;

  //If you made it here and the processID is not 0, check for either a 
  //request or release
  if(id != 0) {
    int request = pcbArray[processLocation].request;
    int release = pcbArray[processLocation].release;
    //If request flag is set, try to give that process the requested resource
    if(request > -1) {
      //If there are available resources, allocate them
      if(resourceArray[request].quantAvail > 0) {
        pcbArray[processLocation].request = -1;
        resourceArray[request].quantAvail -= 1;
        int i;
        int openSpace = -1;
        int foundExisting = 0;
        //See if the process already has that resource, if so, just update the quantity
        for(i = 0; i < 20; i++) {
          if(pcbArray[processLocation].allocation.type[i] == request) {
            pcbArray[processLocation].allocation.quantity[i] += 1; 
            foundExisting = 1;
            break;
          }
          //Set the openSpace var to the first available openSpace
          else {
            if(openSpace == -1) {
              openSpace = i;
            } 
          }
        }
        //Otherwise, give it a new resource
        if(!foundExisting) {
          pcbArray[processLocation].allocation.type[openSpace] = request;
          pcbArray[processLocation].allocation.quantity[openSpace] += 1; 
        }
      } 
    }
    //If the release flag was set, release on of the selected resources
    //and put it back in the resource array
    if(release > -1) {
      pcbArray[processLocation].allocation.quantity[release] -= 1;
      resourceArray[release].quantAvail += 1;
      //If that was the last resource allocated to the process
      //set the type to -1
      if(pcbArray[processLocation].allocation.quantity[release] == 0) {
         pcbArray[processLocation].allocation.type[release] = -1;
      }
    }
  }
  //If the processID is 0, then it is no longer running. Proceed with cleanup
  else {
    printf("%sProcess completed its time%s\n", GRN, NRM);
    fprintf(file, "Process completed its time\n");
    int i;
    int resource;
    //Go through all the allocations to the dead process and put them back into the
    //resource array
    for(i = 0; i < 20; i++) {
      if((resource = pcbArray[processLocation].allocation.type[i]) != -1) {
        resourceArray[resource].quantAvail += pcbArray[processLocation].allocation.quantity[i];
        pcbArray[processLocation].allocation.type[i] = -1;
        pcbArray[processLocation].allocation.quantity[i] = 0;
      } 
    }
    updateAverageTurnaroundTime(processLocation);
    pcbArray[processLocation].totalTimeRan = 0;
    pcbArray[processLocation].createTime = 0;
    pcbArray[processLocation].request = -1;
    pcbArray[processLocation].release = -1;
  }

}

void setupResources(void) {
  int i;
  for(i = 0; i < 20; i++) {
    resourceArray[i].type = i; 
    resourceArray[i].quantity = 1 + rand() % 10;
    resourceArray[i].quantAvail = resourceArray[i].quantity;
  }

  int numShared = 3 + rand() % 3;

  for(i = 0; i < numShared; i++) {
    int choice = rand() % 20;
    resourceArray[choice].quantity = 9999;
    resourceArray[choice].quantAvail = 9999;
  }
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

# o1-koehler.4
This project can be made with the command 'make'
The executable can be ran with './oss' along with any commandline options and arguments you want
Use -h or --help to see the list of available options
The program runs until either 
  1) 20 simulated seconds are reached or
  2) 20 real seconds are reached
Master checks every cycle to see if it can spawn a new process. It checks the gotten future time against the current time. 
If valid, the master then checks to see if any available PCB slots are available. It does this by going through the array of PCB structs in shared memory
  and finding the first one with a PID of 0. It then spawns a new process and assigns it to that location then gets a random time for it to run and a
  priority. 
The master then checkes the queues and schedules the topmost queue (if any) then waits until the scheduled process changes back the scheduler to -1, 
  signaling that it is the master's turn again.
The slave also sends a message back to the master with the location in the PCB array, the master does an 'atoi' operation on that message, then uses
  it to find the location in the array and check to see if the PID has been set to 0 (signaling that the slave has finished its work) then clears the PCB
  after getting the necessary statistics from it. 
The master continues to check to see if it can spawn new processes, enqueue the new ones, and dequeue the highest priority ones until the time runs out.

The times I used to make this work are as follows:

const long long queuePriorityHigh = 40000000;
const long long queuePriorityNormal_1 = 30000000;
const long long queuePriorityNormal_2 = 60000000;
const long long queuePriorityNormal_3 = 120000000;

const long long MAX_TIME = 20000000000;
const int MAX_FUTURE_SPAWN = 280000001;
const int MAX_IDLE_INCREMENT = 10001;
const int MAX_TOTAL_PROCESS_TIME = 700000001;
const int CHANCE_HIGH_PRIORITY = 20;

This seems to give a nice turn around time and decent time inbetween spawn attempts.
I also used the time quantum of the queues as the priority itself to save on additional code of checking against the priority 
  and assigning a time quantum. This way, the process knows the quantum whenever it runs and the master can update the priority 
  as necessary.


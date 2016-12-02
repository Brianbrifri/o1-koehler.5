# o1-koehler.4
This project can be made with the command 'make'
The executable can be ran with './oss' along with any commandline options and arguments you want
Use -h or --help to see the list of available options
The program runs until either 
  1) 20 simulated seconds are reached or
  2) 20 real seconds are reached

This program does message passing to ensure it is a first come first serve on the resources requested; otherwise, resources would be grandted in the same order every time.
  The slaves put their request for new/release of resource into shared memory then sends a message to master. Master processes the message queue recursively then once those are gone, 
  goes through the array of processes and checks for unfulfilled requests. If it can grant the request for a new one, it will. It will always perform the release of a resource by a process.
  I used a struct for the resource that holds the quantity and quantAvail. Whenever one gets assigned to a process, the quantAvail gets decreased. If 
  put back, it gets increased. The processes themselves keep track of which resources it has. 

When a process releases a resource, currently it only chooses the first resource in the array that it has. There is no special determination for which
  one gets released. It does use a random number generator to choose a new process though.

As master is going through and looking for requests to process, it is also keeping track of the processes waiting on resources for the deadlock
  detection. It does not run every 1 second, rather, it runs "in the background" while processing other things.

The times I used to make this work are as follows:

const long long MAX_TIME = 20000000000;
const int MAX_FUTURE_SPAWN = 280000001;
const int MAX_IDLE_INCREMENT = 10000001;



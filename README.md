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

The deadlock detection algorithm runs every second or so. As it does, it marks the processes as deadlocked. Then, it kills of the deadlocked process with the most resources, checks the deadlock again and repeats until no deadlock. 

I also slowed the program down when running in non-verbose mode because the processes did not have enough time to really request many resources. If the verbose flag is set, then I increase the speed of the
  program through the MAX INCREMENT variable. 

NOTE: Processes really don't like to run on hoare. Well, they run but don't grab resources. They work on a local linux envirnonment better.
NOTE 2: If you want to see how the resources are getting allocated/deallocate, it is easier to see when run in verbose mode. If you want to see how the deadlock is working, run in non-verbose mode.


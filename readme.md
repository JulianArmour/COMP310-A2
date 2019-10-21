## COMP 310 - Personal Assignment 2
This assignment demonstrates the use of semaphores in solving the [Readers-Writers synchronization problem](https://en.wikipedia.org/wiki/Readers%E2%80%93writers_problem).

A2Q1.c demonstrates a solution where readers are given priority over writers. But this may lead to readers suffuring from starvation.

A2Q3.c demonstrates how to solve the starvation problem introduced in A2Q1.c by using a queuing semaphore mechanism.

In each program, there are 500 readers and 10 writers trying to gain access to a shared resource (an integer).

Both C file are intended to run on Linux.

They may be compiled as follows:

    $ gcc -o A2Q1.out A2Q1.c -lpthread
They may be ran as follows:

    $ ./A2Q1.out <NumberOfWritesPerThread> <NumberOfReadsPerThread>

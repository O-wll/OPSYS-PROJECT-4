Project Author: Dat Nguyen

Course: Comp Sci 4760

Date: 4/17/2025

GitHub Link: github.com/O-wll/OPSYS4760-PROJECT-4

Description of Project:

This project utilizes a simulated clock and priority message queue. oss.c is the main program that can keep track of queued items, from high priority to low priority, keeping track of how much time was spent in the user processes that it generates at random. It logs all of the activity that is going on in the loop in a log file named oss.log. user.c is a program that simulates doing work in the simulator, it receives a time slice from the main program, oss.c, and then sends a msg about how much time it spent "working."

User will be able to:

View the log file of OSS output, see how much time was spent, what action OSS was doing, etc

How to compile, build, and use project:

The project comes with a makefile so ensure that when running this project that the makefile is in it.

Type 'make' and this will generate both the oss exe and user exe along with their object file.

user exe is for testing of user, you will only need to do ./oss.

When done and want to delete, run 'make clean' and the exe and object files will be deleted.

Issues Ran Into:

Could not fix the fact that sometimes ./oss reaches alarm state and has to execute all processes. 

A bit of confusion on how the program should've worked

Time struggles

General syntax/coding slipups that went unnoticed for a bit.

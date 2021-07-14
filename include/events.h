#ifndef events_h
#define events_h

#include "boundless.h"
#include <pthread.h>
#include <time.h>

struct evt_List {
	struct link_List list;
	pthread_mutex_t mutex;
};

struct evt_Item {
	struct timespec execTime; // When it is to be executed
    int (*func)(); // Function to run
};

int init_events();

void events_close();

// Compares two timespec values and returns true if second is earlier
int evt_compareTimes(struct timespec *first, struct timespec *second);

int evt_addEvent(struct timespec *execTime, int (*func)());

// Runs the next event, if any, from the queue
int evt_runNextEvent();

// Blocks until the next event is ready to be executed
void evt_waitUntilNextEvent();

// Will dedicate a thread to executing events at the correct times
int evt_executeEvents();

/* Start of EVENTS */
// Will print "test" every 5 seconds
int evt_test(); 

#endif

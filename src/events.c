#include "events.h"

struct evt_List events;

pthread_mutex_t timerMutex;
pthread_cond_t timerCond;

// TODO - fix issues with events blocking each other

int init_events(){
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&events.mutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
   	}

    ret = pthread_mutex_init(&timerMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
    }

	ret = pthread_cond_init(&timerCond, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_cond.", ERROR);
        return -1;
    }
	
	evt_userTimeout();
	evt_test();

	return 1;
}

void events_close(){
	// TODO destroy MUTEXES and COND
	return; // Clear out events.list
}

// Compares two timespec values and returns true if second is earlier
int evt_compareTimes(struct timespec *first, struct timespec *second){
	// Check seconds first, because they are bigger
	if(first->tv_sec == second->tv_sec)
		return first->tv_nsec > second->tv_nsec;
	
	return first->tv_sec > second->tv_sec;
}

int evt_addEvent(struct timespec *execTime, int (*func)()){
	if(func == NULL)
		return -1;

	// Create the item struct
	struct evt_Item *item = calloc(1, sizeof(struct evt_Item));
    if(item == NULL){
        log_logError("Error allocating event", DEBUG);
		return -1;
    }

	item->func = func;
	if(execTime == NULL){
		clock_gettime(CLOCK_REALTIME, &item->execTime);
	} else {
		memcpy(&item->execTime, execTime, sizeof(struct timespec));
	}

	// Insert it so that the order of events is correct (earliest -> latest)
	pthread_mutex_lock(&events.mutex);
	struct link_Node *node;
	int pos = 0;
	for(node = events.list.head; node != NULL; node = node->next){
		struct evt_Item *nodeItem = node->data;
		
		// Earlier than current one = insert before
		if(evt_compareTimes(&nodeItem->execTime, &item->execTime) == 1)
			break;

		pos++;
	}

	if(link_insert(&events.list, item, pos) == NULL)
		log_logMessage("Error adding event.", WARNING);
	pthread_mutex_unlock(&events.mutex);

	pthread_cond_signal(&timerCond);

	return 1;
}

// Runs the next event, if any, from the queue
int evt_runNextEvent(){
	struct link_Node *node;
	struct evt_Item *item;

	// Get first event in list
	pthread_mutex_lock(&events.mutex);
	node = events.list.head;

	if(node != NULL)
		item = link_remove(&events.list, 0);
	pthread_mutex_unlock(&events.mutex);
	
	if(item == NULL)
		return -1;

	struct timespec currentTime;
	clock_gettime(CLOCK_REALTIME, &currentTime);
	if(currentTime.tv_sec >= item->execTime.tv_sec){ // Should be executed
		int ret = item->func();
		free(item);
		return ret;
	}

	// Put unused item back
	pthread_mutex_lock(&events.mutex);
	if(link_add(&events.list, item) == NULL)
		log_logMessage("Error adding event.", WARNING);
	pthread_mutex_unlock(&events.mutex);

	return -1;
}

void evt_waitUntilNextEvent(){
	struct timespec execTime;

	// Grab the time from the list
	pthread_mutex_lock(&events.mutex);
	struct link_Node *itemNode = events.list.head;
	if(itemNode != NULL)
		execTime = ((struct evt_Item *) itemNode->data)->execTime;
	pthread_mutex_unlock(&events.mutex);

		
	pthread_mutex_lock(&timerMutex);

	int ret;
	if(itemNode == NULL){ // Wait indefinetly
		ret = pthread_cond_wait(&timerCond, &timerMutex);
	} else {
		ret = pthread_cond_timedwait(&timerCond, &timerMutex, &execTime);
	}

	if(ret != 0 && ret != ETIMEDOUT)
		log_logError("pthread_cond_wait", ERROR);

	pthread_mutex_unlock(&timerMutex);
}

// Will dedicate a thread to executing events at the correct times
int evt_executeEvents(){
	while(1){
		evt_waitUntilNextEvent();
		evt_runNextEvent();
	}

	return 1;
}

/* Start of EVENTS */
// Will print "test" every 5 seconds
int evt_test(){
	log_logMessage("Test event.", EVENT);
	
	struct timespec execTime;
	clock_gettime(CLOCK_REALTIME, &execTime);
	execTime.tv_sec += 5;
	evt_addEvent(&execTime, &evt_test);

	return 1;
}

// Searches for and kicks users that surpassed their message timeouts
int evt_userTimeout(){
	log_logMessage("Removing timed-out users.", EVENT);

	struct timespec execTime;
	clock_gettime(CLOCK_REALTIME, &execTime);
	execTime.tv_sec += fig_Configuration.timeOut;
	evt_addEvent(&execTime, &evt_userTimeout);

	return usr_timeOutUsers(fig_Configuration.timeOut);
}

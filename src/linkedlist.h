#ifndef linkedlist_h
#define linkedlist_h

#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include "logging.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

/* Linked list implementation */

// Data about the list
struct link_List {
	int size;
	struct link_Node *head, *tail;
};

// Data for each node
struct link_Node {
	void *data;
	struct link_Node *next, *prev;
};

int link_isEmpty(struct link_List *list);

// Add element to end of list
struct link_Node *link_add(struct link_List *list, void *data);

/*
 * When using link_remove() only set freeData to 1
 * If the data had been set using malloc or
 * Equivalent
 */
int link_remove(struct link_List *list, int pos, int freeData);

struct link_Node *link_getNode(struct link_List *list, int pos);

#endif

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
 * Returns the pointer to the data
 */
void *link_remove(struct link_List *list, int pos);

int link_indexOf(struct link_List *list, struct link_Node *target);

struct link_Node *link_getNode(struct link_List *list, int pos);

#endif

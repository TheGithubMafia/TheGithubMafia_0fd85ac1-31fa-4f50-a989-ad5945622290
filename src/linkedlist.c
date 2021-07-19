#include <stdio.h>
#include <stdlib.h>
#include "logging.h"
#include "linkedlist.h"

int link_isEmpty(struct link_List *list){
    if(list->head == NULL || list->size <= 0){
            return 1;
    }

    return -1;
}

struct link_Node *link_add(struct link_List *list, void *data){
	return link_insert(list, data, list->size);
}

// Same as link_add, except puts item in specified pos
struct link_Node *link_insert(struct link_List *list, void *data, int pos){
	if(list == NULL)
		return NULL;

    struct link_Node *node = malloc(sizeof(struct link_Node));
    if(node == NULL){
            log_logError("Error adding to linked list", DEBUG);
            return NULL;
    }
	node->data = data;

	struct link_Node *after = link_getNode(list, pos);
	struct link_Node *before = NULL;
	if(after == NULL){ // Put at end of the list
		before = list->tail;	
		node->next = NULL;
		list->tail = node;
	} else {
		before = after->prev;
	}

	if(before == NULL) { // Head of the list
		list->head = node;
	} else {
		before->next = node;
	}

	node->next = after;
	node->prev = before;

    list->size++;
	return node;
}

void *link_remove(struct link_List *list, int pos){
    struct link_Node *node = link_getNode(list, pos);
    if(node == NULL){
		char buff[100];
		snprintf(buff, ARRAY_SIZE(buff), "Position %d does not exist", pos);
		log_logMessage(buff, DEBUG);
		return NULL;
    }

    return link_removeNode(list, node);
}

void *link_removeNode(struct link_List *list, struct link_Node *node){
    if(!link_isEmpty(list)){
            log_logMessage("List is empty: can't remove element", DEBUG);
            return NULL;
    }

    if(node == NULL){
            log_logMessage("Node cannot be NULL", DEBUG);
            return NULL;
    }
    void *data = node->data;

    //Change pointers of elements in front and behind it
    if(node->next != NULL){
            node->next->prev = node->prev;
    }

    if(node->prev != NULL){
            node->prev->next = node->next;
    }

    //pointers of tail and head of list
    if(list->head == node){
            list->head = node->next;
    }
    
    if(list->tail == node){
            list->tail = node->prev;	
    }

    free(node);
    list->size--;
    return data;

}

int link_containsNode(struct link_List *list, struct link_Node *node){
    struct link_Node *n;

    for(n = list->head; n != NULL; n = n->next){
        if(node == n){
            return 1;
        }
    }

    return -1;
}

int link_indexOf(struct link_List *list, struct link_Node *target){
    int index = -1;
    struct link_Node *node;

    int pos = 0;
    for(node = list->head; node != NULL; node = node->next){
       if(node == target){
            index = pos;
            break;
       }

       pos++;
    }

    return index;
}

struct link_Node *link_getNode(struct link_List *list, int pos){
    if(list->head == NULL || pos < 0){
            return NULL;
    }

    struct link_Node *res = list->head;
    for(int i = 0; i < pos; i++){
            res = res->next;

            if(res == NULL){
                    return NULL;
            }
    }

    return res;
}

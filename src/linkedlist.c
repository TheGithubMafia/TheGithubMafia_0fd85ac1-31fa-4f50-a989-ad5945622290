#include <stdio.h>
#include <stdlib.h>
#include "logging.h"
#include "linkedlist.h"

int link_isEmpty(struct link_List *list){
    if(list->head == NULL || list->size <= 0){
            return 0;
    }

    return -1;
}

struct link_Node *link_add(struct link_List *list, void *data){
    struct link_Node *node = malloc(sizeof(struct link_Node));
    if(node == NULL){
            log_logError("Error adding to linked list", DEBUG);
            return NULL;
    }
    list->size++;

    node->data = data;
    node->next = NULL;
    if(!link_isEmpty(list)){ // Is the head if list is empty
            list->head = node;
            node->prev = NULL;
    } else { //Only works if isn't the head 
            node->prev = list->tail;
            list->tail->next = node;
    }
    list->tail = node;

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

#ifndef chat_h
#define chat_h

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define NICKNAME_LENGTH 10
#define CHANNEL_NAME_LENGTH 201

/*  Note about the structure of the users
	All new users are added to the main linked
	list via malloc. All other uses to users should
	access the user through a pointer to the pointer
	inside the main list so that one free() will notify
	all other pointers that the user no longer exists
*/
struct chat_ServerLists {
	int max;
	struct link_List users;	
	pthread_mutex_t usersMutex;
	struct link_List servers;	
	pthread_mutex_t serversMutex;
};

// Data about an user
// When a user is first loaded from save
// All details will come from the save
// except socketInfo, it must filled with 0 bytes
// except with socketInfo.socket must equal -1
struct chat_UserData {
	size_t id;
	struct com_SocketInfo socketInfo;	
	char name[NICKNAME_LENGTH];
    char input[1024];
    char output[1024];
	pthread_mutex_t userMutex;
};

// Server is a list of Channels
struct chat_Server {
	size_t id;
	char name[50];
	struct link_List users, rooms;
	pthread_mutex_t serverMutex;
};

struct chat_Channel {
	size_t id;
	char name[CHANNEL_NAME_LENGTH];
	struct link_List users;
	pthread_mutex_t roomMutex;
};

struct chat_DataQueue {
    struct link_List queue;
    pthread_t *threads;
    pthread_mutex_t queueMutex;
};

void chat_setMaxUsers(int max);

int init_chat();

void chat_close();

// Setup threads for data processing
int chat_setupDataThreads(struct fig_ConfigData *config);

// Insert selected node into the queue for processing
// Mutex is handled by this function internally
int chat_insertQueue(struct link_Node *node);

// Process the queue's contents and then send data back
// to the communication queue for sending back to clients
void *chat_processQueue(void *param);

// Parse the input from a user and act on it
int chat_parseInput(struct link_Node *node);

//Get a user's node in the main list by name
struct link_Node *chat_getUserByName(char name[NICKNAME_LENGTH]);

//Get a user's node in the main list by ID
struct link_Node *chat_getUserById(size_t id);

//Get a user's node in the main list by Socket FD
struct link_Node *chat_getUserBySocket(int sock);

//Create a new user, but only if it doesn't already exist
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char name[NICKNAME_LENGTH]);

// Returns the node to a new user, also automatically adds the user to the main list
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char *name);

// Places a double pointer to the user into the Channel's list
struct link_Node *chat_addToChannel(struct chat_Channel *room, struct link_Node *user);

// Sends a message to all online users in this room
int chat_sendChannelMsg(struct chat_Channel *room, char *msg, int msgSize);

#endif

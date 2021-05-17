#ifndef chat_h
#define chat_h

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"
#include "commands.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))
#define NICKNAME_LENGTH 9
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
	struct link_List groups;	
	pthread_mutex_t groupsMutex;
        struct link_List channels;
        pthread_mutex_t channelsMutex;
};

// Data about an user
// When a user is first loaded from save
// All details will come from the save
// except socketInfo, it must filled with 0 bytes
// except with socketInfo.socket must equal -1
struct chat_UserData {
	size_t id;
	struct com_SocketInfo socketInfo;	
	char nickname[NICKNAME_LENGTH + 1];
	char input[1024];
	char output[1024];
	pthread_mutex_t userMutex;
};

// New feature: A group a channels that a user can join
// All at once, and an operator has full control over all
struct chat_Group {
    char name[CHANNEL_NAME_LENGTH];
    struct link_List channels;
    pthread_mutex_t groupMutex;
};

struct chat_Channel {
	size_t id;
	char name[CHANNEL_NAME_LENGTH];
	struct link_List users;
	pthread_mutex_t channelMutex;
};

struct chat_DataQueue {
    struct link_List queue;
    pthread_t *threads;
    pthread_mutex_t queueMutex;
};

// Contains all parts of a typical message
// The userNode is used to identify the user when receiving
// and used to identify the recipient when sending
struct chat_Message {
    struct link_Node *userNode;
    char prefix[50];
    char command[50];
    int paramCount;
    char params[10][400];
};

void chat_setMaxUsers(int max);

int init_chat();

void chat_close();

// Setup threads for data processing
int chat_setupDataThreads(struct fig_ConfigData *config);

// Insert selected node into the queue for processing
// Mutex is handled by this function internally
int chat_insertQueue(struct com_QueueJob *job);

// Process the queue's contents and then send data back
// to the communication queue for sending back to clients
void *chat_processQueue(void *param);

// Parse the input from a user and act on it
int chat_parseInput(struct com_QueueJob *job);

// Will send a Message struct to specified node
int chat_sendMessage(struct chat_Message *msg);

// Fills in a Message struct
int chat_createMessage(struct chat_Message *msg, struct link_Node *user, char *prefix, char *cmd, char **params, int paramCount);

// Converts a message struct into a string form suitable for sending
int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr);

// Locate the next space character
int chat_findNextSpace(int starting, int size, char *str);

// Fill in a struct with a user's node
int chat_getNameByNode(char buff[NICKNAME_LENGTH], struct link_Node *userNode);

//Get a user's node in the main list by name
struct link_Node *chat_getUserByName(char name[NICKNAME_LENGTH]);

//Get a user's node in the main list by ID
struct link_Node *chat_getUserById(size_t id);

//Get a user's node in the main list by Socket FD
struct link_Node *chat_getUserBySocket(int sock);

// Returns the node to a new user, also automatically adds the user to the main list
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char name[NICKNAME_LENGTH]);

// Returns the node to a channel if it exists
struct link_Node *chat_getChannelByName(char *name);

// Create a channel with the specified name, and add it to the specified group
struct link_Node *chat_createChannel(char *name, struct chat_Group *group);

// check if a user is in a channel
int chat_isInChannel(struct link_Node *channelNode, struct link_Node *userNode);

// Places a pointer to the user into the Channel's list, and create it if needed
struct link_Node *chat_addToChannel(struct link_Node *channelNode, struct link_Node *userNode);

// Will fill a buffer with list of users
int chat_getUsersInChannel(struct link_Node *channelNode, char *buff, int size);

// Sends a message to all online users in this room
int chat_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode);

#endif

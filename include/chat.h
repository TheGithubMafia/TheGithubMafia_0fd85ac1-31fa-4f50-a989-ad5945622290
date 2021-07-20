#ifndef chat_h
#define chat_h

#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"
#include "commands.h"
#include "user.h"
#include "channel.h"
#include "group.h"

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

#define TYPE_USER 0
#define TYPE_CHAN 1
#define TYPE_GROUP 2

#define NUM_MODES 15

/*  Note about the structure of the users
    All new users are added to the main linked
    list via malloc. All other uses to users should
    access the user through a pointer to the pointer
    inside the main list so that one free() will notify
    all other pointers that the user no longer exists
*/
struct chat_ServerLists {
	int max;
	int connected;
	struct usr_UserData *users;
	struct link_List groups;	
	pthread_mutex_t groupsMutex;
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
    struct usr_UserData *user;
    char prefix[50];
    char command[50];
    int paramCount;
    char params[10][400];
};

// So all functions can access this global list
extern struct chat_ServerLists serverLists;

int init_chat();

void chat_close();

int chat_serverIsFull();

// Setup threads for data processing
int chat_setupDataThreads(struct fig_ConfigData *config);

// Give data about a job and insert it into the queue
int chat_insertQueue(struct usr_UserData *user, int type, char *str, struct chat_Message *msg);

// Process the queue's contents and then send data back
// to the communication queue for sending back to clients
void *chat_processQueue(void *param);

// Parse the input from a user and act on it
int chat_parseInput(struct com_QueueJob *job);

// Will send a Message struct to specified node
int chat_sendMessage(struct chat_Message *msg);

// Sends a message to every connected user
int chat_sendServerMessage(struct chat_Message *cmd);

// Fills in a Message struct
int chat_createMessage(struct chat_Message *msg, struct usr_UserData *user, char *prefix, char *cmd, char **params, int paramCount);

// Converts a message struct into a string form suitable for sending
int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr);

// Locate the next space character
int chat_findNextSpace(int starting, int size, char *str);

// Returns the location of either \n or \r
int chat_findEndLine(char *str, int size, int starting);

// General character location
int chat_findCharacter(char *str, int size, char key);

// Divide a string into groupname and channelname
int chat_divideChanName(char *str, int size, char data[2][1000]);

// Checks if a given mode is valid
int chat_isValidMode(char mode, int type);

#endif

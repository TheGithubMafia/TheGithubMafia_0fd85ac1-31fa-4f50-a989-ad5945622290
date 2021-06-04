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
#define UNREGISTERED_NAME "unreg"

/*  Note about the structure of the users
    All new users are added to the main linked
    list via malloc. All other uses to users should
    access the user through a pointer to the pointer
    inside the main list so that one free() will notify
    all other pointers that the user no longer exists
*/
struct chat_ServerLists {
	int max;
	struct chat_UserData *users;
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
	int id;
	int admin;
	struct com_SocketInfo socketInfo;	
	char nickname[NICKNAME_LENGTH + 1];
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
    struct chat_UserData *user;
    char prefix[50];
    char command[50];
    int paramCount;
    char params[10][400];
};

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

// Sends a message to every connected user
int chat_sendServerMessage(struct chat_Message *cmd);

// Fills in a Message struct
int chat_createMessage(struct chat_Message *msg, struct chat_UserData *user, char *prefix, char *cmd, char **params, int paramCount);

// Converts a message struct into a string form suitable for sending
int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr);

// Locate the next space character
int chat_findNextSpace(int starting, int size, char *str);

// Fills in buffer with selected user's nickname
int chat_getNickname(char buff[NICKNAME_LENGTH], struct chat_UserData *user);

//Get a user by name
struct chat_UserData *chat_getUserByName(char name[NICKNAME_LENGTH]);

//Get a user by id
struct chat_UserData *chat_getUserById(int id);

//Get a user by socket fd
struct chat_UserData *chat_getUserBySocket(int sock);

// Returns a new user, after adding them to the main list
struct chat_UserData *chat_createUser(struct com_SocketInfo *sockInfo, char *name);

// Remove a user from the server
int chat_deleteUser(struct chat_UserData *user);

int chat_userIsRegistered(struct chat_UserData *user);

int chat_removeUserFromChannel(struct link_Node *channelNode, struct chat_UserData *user);

int chat_removeUserFromAllChannels(struct chat_UserData *user);

// Returns the node to a channel if it exists
struct link_Node *chat_getChannelByName(char *name);

// Create a channel with the specified name, and add it to the specified group
struct link_Node *chat_createChannel(char *name, struct chat_Group *group);

// check if a user is in a channel
int chat_isInChannel(struct link_Node *channelNode, struct chat_UserData *user);

// Places a pointer to the user into the Channel's list, and create it if needed
struct link_Node *chat_addToChannel(struct link_Node *channelNode, struct chat_UserData *user);

// Will fill a buffer with list of nicknames
int chat_getUsersInChannel(struct link_Node *channelNode, char *buff, int size);

// Sends a message to all online users in this room
int chat_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode);

#endif

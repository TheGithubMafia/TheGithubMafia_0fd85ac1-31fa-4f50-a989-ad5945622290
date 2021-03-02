#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"
#ifndef chat_h
#define chat_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

/*  Note about the structure of the users
	All new users are added to the main linked
	list via malloc. All other uses to users should
	access the user through a pointer to the pointer
	inside the main list so that one free() will notify
	all other pointers that the user no longer exists
*/
struct chat_AllUsers {
	int max;
	struct link_List users;	
	pthread_mutex_t allUsersMutex;
};

// Data about an user
struct chat_UserData {
	size_t id;
	struct com_SocketInfo socketInfo;	
	char name[26];
	pthread_mutex_t userMutex;
};

struct chat_Server {
	size_t id;
	char name[50];
	struct link_List users, rooms;
	pthread_mutex_t serverMutex;
};

struct chat_ChatRoom {
	size_t id;
	char name[50];
	struct link_List users;
	pthread_mutex_t roomMutex;
};

void chat_setMaxUsers(int max);

// Returns the node to a new user, also automatically adds the user to the main list
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char *name);

// Places a double pointer to the user into the Room's list
struct link_Node *chat_addToRoom(struct chat_ChatRoom *room, struct link_Node *user);

// Sends a message to all online users in this room
int chat_sendRoomMsg(struct chat_ChatRoom *room, char *msg, int msgSize);

#endif

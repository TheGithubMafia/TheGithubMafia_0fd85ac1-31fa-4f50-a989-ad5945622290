#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "linkedlist.h"
#ifndef chat_h
#define chat_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

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

int chat_addToRoom(struct chat_ChatRoom *room, struct chat_UserData *user);

int chat_sendRoomMsg(struct chat_ChatRoom *rom, char *msg, int msgSize);

#endif

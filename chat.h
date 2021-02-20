#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#ifndef chat_h
#define chat_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

// Data about an user
struct chat_UserData {
	size_t id;
	struct com_SocketInfo socket;	
	char name[26];
	pthread_mutex_t userMutex;
};

struct chat_Server {
	size_t id;
	char name[50];
	// linked list of chatrooms
	// and users
	struct chat_UserData users[50];
	pthread_mutex_t serverMutex;
};

struct chat_ChatRoom {
	size_t id;
	char name[50];
	//find better way to set users 
	//without using so much memory
	struct chat_UserData users[50];
	pthread_mutex_t roomMutex;
};

int chat_addToRoom(struct chat_Room room, struct chat_UserData user);

#endif

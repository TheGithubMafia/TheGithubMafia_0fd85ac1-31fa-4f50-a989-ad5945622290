#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"

int chat_CreateUser(struct chat_UserData userList[], struct com_SocketInfo *sockInfo, char* name){
	// Look for the first empty spot in the userList
	// The 0th spot is reserved for the list's mutex
	pthread_mutex_lock(&userList[0].userMutex);
	int res = -1;
	for (int i = 1; i < ARRAY_SIZE(userList); i++){
		if(userList[i].id <= 0){
			res = i;
			break;
		}
	}

	if(res < 0){
		pthread_mutex_unlock(&userList[0].userMutex);
		log_logMessage("Can't add uesr: Server full", ERROR);
		return -1;
	}

	userList[res].id = res;
	strncpy(userList[res].name, name, ARRAY_SIZE(userList[res].name - 1));
	userList


	return 0;
}

//Maybe consider adding user mutex
int chat_addToRoom(struct chat_ChatRoom *room, struct chat_UserData *user){
	pthread_mutex_lock(&room->roomMutex);
	int ret = link_add(&room->users, user);
	pthread_mutex_unlock(&room->roomMutex);

	return ret;
}

int chat_sendRoomMsg(struct chat_ChatRoom *room, char *msg, int msgSize){
	struct link_Node *node;
	struct chat_UserData *user;
	int ret;

	if(room == NULL){
		log_logError("Invalid room");
		return -1;
	}

	pthread_mutex_lock(&room->roomMutex);
	for(node = room->users.head; node != NULL; node = node->next){
		user = (struct chat_UserData *)node->data;
		if(user == NULL){
			continue;
		}

		pthread_mutex_lock(&user->userMutex);
		if(user->socketInfo.socket >= 0){//Do nothing if negative socket fd
			ret = write(user->socketInfo.socket, msg, msgSize);
			if(ret < 1){
				log_logError("Error sending to client", DEBUG);
			}
		}
		pthread_mutex_unlock(&user->userMutex);
	}

	pthread_mutex_unlock(&room->roomMutex);

	return 0;
}


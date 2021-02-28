#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"

struct chat_AllUsers allUsers = {0};
size_t chat_globalUserID = 0;

void chat_setMaxUsers(int max){
	allUsers.max = max;
}

struct chat_UserData *chat_createUser(struct com_SocketInfo *sockInfo, char* name){
	struct chat_UserData *user;

	pthread_mutex_lock(&allUsers.allUsersMutex);
	if(allUsers.users.size >= allUsers.max){
		log_logMessage("Server is full", WARNING);
		pthread_mutex_unlock(&allUsers.allUsersMutex);	
		return NULL;
	}

	user = malloc(sizeof(struct chat_UserData));
	if(user == NULL){
		log_logError("Error adding user", ERROR);
		pthread_mutex_unlock(&allUsers.allUsersMutex);	
		return NULL;
	}
	//Set user's data
	memset(user, 0, sizeof(struct chat_UserData));
	//eventually get this id from saved user data
	user->id = chat_globalUserID++;
	strncpy(user->name, name, 25);
	memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));

	link_add(&allUsers.users, user);

	pthread_mutex_unlock(&allUsers.allUsersMutex);	

	return user;
}

// Add a user to a room
int chat_addToRoom(struct chat_ChatRoom *room, struct chat_UserData **user){
	pthread_mutex_lock(&room->roomMutex);
	int ret = link_add(&room->users, *user);
	pthread_mutex_unlock(&room->roomMutex);

	return ret;
}

// Make the double pointer work so that it will become null when the user is invalid
int chat_sendRoomMsg(struct chat_ChatRoom *room, char *msg, int msgSize){
	struct link_Node *node;
	struct chat_UserData *user;
	int ret;

	if(room == NULL){
		log_logMessage("Invalid room", WARNING);
		return -1;
	}

	pthread_mutex_lock(&room->roomMutex);
	// Loop through each user in the room and send them the message
	for(node = room->users.head; node != NULL; node = node->next){
		user = (struct chat_UserData *)node->data;
		if(user == NULL){
			continue;
		}

		pthread_mutex_lock(&(*user).userMutex);
		if((*user).socketInfo.socket >= 0){//Do nothing if negative socket fd
			ret = write((*user).socketInfo.socket, msg, msgSize);//SOme reason is same fd?
			if(ret < 1){
				log_logError("Error sending to client", DEBUG);
			}
		}
		pthread_mutex_unlock(&(*user).userMutex);
	}

	pthread_mutex_unlock(&room->roomMutex);

	return 0;
}

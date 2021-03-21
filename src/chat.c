#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"

struct chat_ServerLists serverLists = {0};
size_t chat_globalUserID = 0;

struct chat_DataQueue dataQueue;

void chat_setMaxUsers(int max){
	serverLists.max = max;
}

int init_chat(){
    //Check that the config data is correct
	if(fig_Configuration.threadsDATA < 1){
		fig_Configuration.threadsDATA = 1;
		log_logMessage("Must have at least 1 data thread! Using 1 data thread", WARNING);
	}	

    // Allocate threads for processing user input 
    dataQueue.threads = calloc(fig_Configuration.threadsDATA, sizeof(pthread_t)); 
    if (dataQueue.threads == NULL){
        log_logError("Error initalizing dataQueue.threads", ERROR);
        return -1;
    }
    
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&dataQueue.queueMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex", ERROR);
        return -1;
    }

    return chat_setupDataThreads(&fig_Configuration); 
}

void chat_close(){
    free(dataQueue.threads);
}

int chat_setupDataThreads(struct fig_ConfigData *config){
    char buff[BUFSIZ];
    int numThreads = config->threadsDATA;
    int ret = 0;

    for (int i = 0; i < numThreads; i++){
        ret = pthread_create(&dataQueue.threads[i], NULL, chat_processQueue, &dataQueue);
        if (ret < 0){
            log_logError("Error initalizing thread", ERROR);
            return -1;
        }
    }

    snprintf(buff, ARRAY_SIZE(buff), "Successfully processing data on %d threads", numThreads);
    log_logMessage(buff, INFO);

    return numThreads;
}

int chat_insertQueue(struct link_Node *node){
    pthread_mutex_lock(&dataQueue.queueMutex); 
    link_add(&dataQueue.queue, node);
    pthread_mutex_unlock(&dataQueue.queueMutex); 

    return 1;
}

void *chat_processQueue(void *param){
    struct chat_DataQueue *dataQ = param;
    struct timespec delay = {.tv_nsec = 1000000}; // 1ms

    while(1) { 
        struct link_Node *node = NULL;

        // grab from first item in linked list: expecting a link_Node of the user
        // also make sure list isn't empty
        pthread_mutex_lock(&dataQ->queueMutex);
        if(link_isEmpty(&dataQ->queue) < 0){
            node = link_remove(&dataQ->queue, 0);
        }
        pthread_mutex_unlock(&dataQ->queueMutex);

        // Nothing to process
        if(node == NULL){
            nanosleep(&delay, NULL); // Allow other threads time to access mutex
            continue;
        }

        chat_parseInput(node); 
    }

    return NULL;
}

// TODO - Support multiple cmds in one read
// TODO - Handle null bytes? also handle MAJOR issues with memcpy (size of copied)
int chat_parseInput(struct link_Node *node){
    struct chat_UserData *user;
    user = (struct chat_UserData *) node->data;
    struct chat_Message cmd = {0};

    pthread_mutex_lock(&user->userMutex); 

    log_logMessage(user->input, MESSAGE);

    // Find where the message ends (\r or \n); if not supplied just take the very end of the buffer
    int length = ARRAY_SIZE(user->input) - 1;
    int currentPos = 0, loc = 0; // Helps to keep track of where string should be copied
    for (int i = 0; i < ARRAY_SIZE(user->input); i++){
        if(user->input[i] == '\n' || user->input[i] == '\r'){
            length = i+1;
            user->input[i] = ' ';
            break;
        }
    }

    if(user->input[0] == ':'){
       loc = chat_findNextSpace(0, length, user->input);
       if (loc > -1){
           memcpy(cmd.prefix, &user->input[1], loc - 1); // Copy everything except for the ':'
           currentPos = loc + 1;
       }
    }

    loc = chat_findNextSpace(currentPos, length, user->input); 
    if (loc >= 0){
        memcpy(cmd.command, &user->input[currentPos], loc - currentPos);
        currentPos = loc + 1;
    }

    // Fill in up to 15 params
    while (loc > -1 && cmd.paramCount < 15){
       loc = chat_findNextSpace(currentPos, length, user->input);
       if (loc >= 0){
            memcpy(cmd.params[cmd.paramCount], &user->input[currentPos], loc - currentPos);
            currentPos = loc +1;
            cmd.paramCount++;
       }
    }

    snprintf(user->output, ARRAY_SIZE(user->output), "invalid command\r\n");
    if(strncmp(cmd.command, "NICK", ARRAY_SIZE(cmd.command)) == 0){
        strncpy(user->nickname, cmd.params[0], NICKNAME_LENGTH);
        snprintf(user->output, ARRAY_SIZE(user->output), ":localhost 001 %s :Welcome to server\r\n", cmd.params[0]);
    }

    com_insertQueue(node);
    pthread_mutex_unlock(&user->userMutex);

    return 1;
}

// Locate the next space character 
int chat_findNextSpace(int starting, int size, char *str){
    for(int i = starting; i < size; i++){
        if(str[i] == ' ' || str[i] == '\n' || str[i] == '\r'){
            return i;
        }
    }

    return -1;
}

//Same as other but uses name to find answer
struct link_Node *chat_getUserByName(char name[NICKNAME_LENGTH]){
	struct link_Node *node;
	struct chat_UserData *user;

	pthread_mutex_lock(&serverLists.usersMutex);

	for(node = serverLists.users.head; node != NULL; node = node->next){
		user = node->data;
		pthread_mutex_lock(&user->userMutex);

		//compare the names letter by letter
		for(int i = 0; i < ARRAY_SIZE(user->nickname); i++){
			if(user->nickname[i] != name[i]){
				break;
			}

			pthread_mutex_unlock(&user->userMutex);
			pthread_mutex_unlock(&serverLists.usersMutex);
			return node;
		}
		
		pthread_mutex_unlock(&user->userMutex);
	}

	pthread_mutex_unlock(&serverLists.usersMutex);

	return NULL;
}

//Find the user in the serverLists list using the user id
struct link_Node *chat_getUserBySocket(int sock){
	struct link_Node *node;
	struct chat_UserData *user;

	pthread_mutex_lock(&serverLists.usersMutex);

	for(node = serverLists.users.head; node != NULL; node = node->next){
		user = node->data;
		pthread_mutex_lock(&user->userMutex);

		if(user->socketInfo.socket == sock){
			pthread_mutex_unlock(&user->userMutex);
			pthread_mutex_unlock(&serverLists.usersMutex);
			return node;
		}

		pthread_mutex_unlock(&user->userMutex);
	}

	pthread_mutex_unlock(&serverLists.usersMutex);

	return NULL;
}

//Find the user in the serverLists list using the user id
struct link_Node *chat_getUserById(size_t id){
	struct link_Node *node;
	struct chat_UserData *user;

	pthread_mutex_lock(&serverLists.usersMutex);

	for(node = serverLists.users.head; node != NULL; node = node->next){
		user = node->data;
		pthread_mutex_lock(&user->userMutex);

		if(user->id == id){
			pthread_mutex_unlock(&user->userMutex);
			pthread_mutex_unlock(&serverLists.usersMutex);
			return node;
		}

		pthread_mutex_unlock(&user->userMutex);
	}

	pthread_mutex_unlock(&serverLists.usersMutex);

	return NULL;
}

struct link_Node *chat_loginUser(struct com_SocketInfo *sockInfo, char name[NICKNAME_LENGTH]){
	struct link_Node *node = chat_getUserByName(name);

	if(node == NULL){
		return NULL;
	}

	struct chat_UserData *user = node->data;
	pthread_mutex_lock(&(user->userMutex));

	memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));
	
	pthread_mutex_lock(&(user->userMutex));

	return node;
}

//Create a new user and return the node that it is in
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char name[NICKNAME_LENGTH]){
	struct chat_UserData *user;

	pthread_mutex_lock(&serverLists.usersMutex);
	//Plan to remove this maxUser check because it should be only
	//for connected users
	if(serverLists.users.size >= serverLists.max){
		log_logMessage("Server is full", WARNING);
		pthread_mutex_unlock(&serverLists.usersMutex);	
		return NULL;
	}

	user = malloc(sizeof(struct chat_UserData));
	if(user == NULL){
		log_logError("Error adding user", ERROR);
		pthread_mutex_unlock(&serverLists.usersMutex);	
		return NULL;
	}
	//Set user's data
	memset(user, 0, sizeof(struct chat_UserData));
	//eventually get this id from saved user data
	user->id = chat_globalUserID++;
	strncpy(user->nickname, name, NICKNAME_LENGTH);
	memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));

	struct link_Node *userNode = link_add(&serverLists.users, user);

	pthread_mutex_unlock(&serverLists.usersMutex);	

	return userNode;
}

// Add a user to a channel from their node on the main user list
struct link_Node *chat_addToChannel(struct chat_Channel *channel, struct link_Node *user){
	pthread_mutex_lock(&channel->channelMutex);
	struct link_Node *ret = link_add(&channel->users, user);
	pthread_mutex_unlock(&channel->channelMutex);

	return ret;
}

// TODO - rewrite to add users to the queue
int chat_sendChannelMsg(struct chat_Channel *room, char *msg, int msgSize){
	struct link_Node *node;
	struct chat_UserData **user;
	int ret;

	if(room == NULL){
		log_logMessage("Invalid room", WARNING);
		return -1;
	}

	pthread_mutex_lock(&room->channelMutex);
	// Loop through each user in the room and send them the message
	for(node = room->users.head; node != NULL; node = node->next){
		user = (struct chat_UserData **)node->data;
		if(user == NULL){
			//Add remove user function: invalid user
			continue;
		}

		pthread_mutex_lock(&(**user).userMutex);

		if((**user).socketInfo.socket < 0){
			//deal with offline user
			continue;
		}

		if((**user).socketInfo.socket >= 0){//Do nothing if negative socket fd
			ret = write((**user).socketInfo.socket, msg, msgSize);
			if(ret < 1){
				log_logError("Error sending to client", DEBUG);
			}
		}
		pthread_mutex_unlock(&(**user).userMutex);
	}

	pthread_mutex_unlock(&room->channelMutex);

	return 0;
}

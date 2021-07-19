#include "user.h"

size_t usr_globalUserID = 0;
const char usr_userModes[] = {'i', 'o', 'r', 'a'};

int usr_getNickname(char *buff, struct usr_UserData *user){
    if(user == NULL || user->id < 0){
        return -1;
    }

    pthread_mutex_lock(&user->userMutex);
    strncpy(buff, user->nickname, fig_Configuration.nickLen);
    pthread_mutex_unlock(&user->userMutex);

    return 1;
}

struct usr_UserData *usr_getUserByName(char *name){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->userMutex);

            if(user->id >= 0 && !strncmp(user->nickname, name, fig_Configuration.nickLen)){
                    pthread_mutex_unlock(&user->userMutex);
                    return user;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    return NULL;
}

struct usr_UserData *usr_getUserBySocket(int sock){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->userMutex);

            if(user->socketInfo.socket == sock){
                    pthread_mutex_unlock(&user->userMutex);
                    return user;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    return NULL;
}

struct usr_UserData *usr_getUserById(int id){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->userMutex);

            if(user->id == id){
                    pthread_mutex_unlock(&user->userMutex);
                    return user;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    return NULL;
}

//Create a new user and return it
// TODO fix double nicks if two clients request same name slightly different times
struct usr_UserData *usr_createUser(struct com_SocketInfo *sockInfo, char *name){
    struct usr_UserData *user;
	int success = -1;

	// Find an empty spot
    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];
            pthread_mutex_lock(&user->userMutex);

			if(user->id < 0){
				success = 1;
				break;
			}

            pthread_mutex_unlock(&user->userMutex);
    }

	if(success == -1) { // Failed to find a spot
		log_logMessage("No spots avaliable for new user", ERROR);
		return NULL;
	}

    //Set user's data
    memset(user, 0, sizeof(struct usr_UserData));

	// Allocate necesary data for the user's nickname
	user->nickname = calloc(fig_Configuration.nickLen, sizeof(char));
	if(user->nickname == NULL){
		log_logError("Error allocating user memory", ERROR);
		return NULL;
	}

	memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));
	usr_changeUserMode(user, '+', 'r');
	user->lastMsg = time(NULL); // Starting time
	user->pinged = 0; // Dont ping on registration, but still kick if idle

    //eventually get this id from saved user data
    user->id = usr_globalUserID++;

	// Do this last to ensure user isn't selected before it is ready to be used
    strncpy(user->nickname, name, fig_Configuration.nickLen);

    return user;
}

int usr_deleteUser(struct usr_UserData *user){
	if(user == NULL)
		return -1;

	printf("Deleting User\n");
		printf("c%p\n", user);
    // Nothing new will be sent to queue
    pthread_mutex_lock(&user->userMutex);
    user->id = -1; // -1 means invalid user
	if(user->nickname != NULL)
		free(user->nickname);

    // Remove socket
	close(user->socketInfo.socket);
    user->socketInfo.socket = -2; // Ensure that no data sent to wrong user
	close(user->socketInfo.socket2);
    user->socketInfo.socket2 = -2; // Ensure that no data sent to wrong user

    pthread_mutex_unlock(&user->userMutex);

    // Remove all pending messages
    com_cleanQueue(user);

    // Channels
    chan_removeUserFromAllChannels(user);

    // Groups

    return 1;
}

// Searches for and kicks users that surpassed their message timeouts
int usr_timeOutUsers(int timeOut){
    struct usr_UserData *user;

    for(int i = 0; i < serverLists.max; i++){
            user = &serverLists.users[i];

            pthread_mutex_lock(&user->userMutex);
			int id = user->id;
			int diff = (int) difftime(time(NULL), user->lastMsg);
			int pinged = user->pinged;
			pthread_mutex_unlock(&user->userMutex);

			if(id != -1 && id != 0){ // Neither invalid nor SERVER
				if(diff > timeOut){
					log_logMessage("User timeout.", INFO);
					usr_deleteUser(user);	
				} else if(pinged == -1 && diff > timeOut/2){ // Ping user
					com_sendStr(user, "PING :Timeout imminent.");

					pthread_mutex_lock(&user->userMutex);
					user->pinged = 1;
					pthread_mutex_unlock(&user->userMutex);
				}
			}
    }

    return 1;
}

void usr_changeUserMode(struct usr_UserData *user, char op, char mode){
	if(user == NULL || user->id < 0){
		return;
	}

	// No duplicates allowed
	if(op == '+' && usr_userHasMode(user, mode) == 1){
		return;
	}

	pthread_mutex_lock(&user->userMutex);
	for(int i = 0; i < ARRAY_SIZE(user->modes); i++){
		if(op == '-' && user->modes[i] == mode){
			user->modes[i] = '\0';
			break;
		} else if(op == '+' && user->modes[i] == '\0'){
			user->modes[i] = mode;
			break;
		}
	}
	pthread_mutex_unlock(&user->userMutex);
}

int usr_userHasMode(struct usr_UserData *user, char mode){
	if(user == NULL || user->id < 0){
		return -1;
	}

	int ret = -1;
	pthread_mutex_lock(&user->userMutex);
	for(int i = 0; i < ARRAY_SIZE(user->modes); i++){
		if(user->modes[i] == mode){
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&user->userMutex);

	return ret;
}

int usr_isUserMode(char mode){
	for(int i = 0; i < ARRAY_SIZE(usr_userModes); i++){
		if(mode == usr_userModes[i]){
			return 1;
		}
	}
	
	return -1;
}

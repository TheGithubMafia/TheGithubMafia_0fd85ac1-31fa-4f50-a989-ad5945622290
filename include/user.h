#ifndef user_h
#define user_h

#include "chat.h"
#include <time.h>

#define UNREGISTERED_NAME "unreg"

// Data about an user
// When a user is first loaded from save
// All details will come from the save
// except socketInfo, it must filled with 0 bytes
// except with socketInfo.socket must equal -1
struct usr_UserData {
	int id;
	char modes[5];
	struct com_SocketInfo socketInfo;	
	char *nickname;
	pthread_mutex_t userMutex;
	time_t lastMsg; // Keep track of time, too fast = kick, too slow = kick
	int pinged; // Send only one ping to prevent spam from server
};

// Fills in buffer with selected user's nickname
int usr_getNickname(char *buff, struct usr_UserData *user);

//Get a user by name
struct usr_UserData *usr_getUserByName(char *name);

//Get a user by id
struct usr_UserData *usr_getUserById(int id);

//Get a user by socket fd
struct usr_UserData *usr_getUserBySocket(int sock);

// Returns a new user, after adding them to the main list
struct usr_UserData *usr_createUser(struct com_SocketInfo *sockInfo, char *name);

// Remove a user from the server
int usr_deleteUser(struct usr_UserData *user);

// Searches for and kicks users that surpassed their message timeouts
int usr_timeOutUsers(int timeOut);

// Adds or removes a mode from a user
void usr_changeUserMode(struct usr_UserData *user, char op, char mode);

// Checks if a user has a mode active
int usr_userHasMode(struct usr_UserData *user, char mode);

int usr_isUserMode(char mode);

#endif

#ifndef group_h
#define group_h

#include "boundless.h"
#include "channel.h"
#include "user.h"

// Data about a user specific to a group
struct grp_GroupUser {
	struct usr_UserData *user;
	int permLevel; // 0 - Default, 1 - groupop
};

// New feature: A group a channels that a user can join
// All at once, and an operator has full control over all
struct grp_Group {
    char *name;
	char modes[NUM_MODES];
	char key[20];
    struct link_List channels;
	int max;
	struct grp_GroupUser *users;
    pthread_mutex_t groupMutex;
};

struct chan_Channel;

// Creates a new group, adds it to the main list, and creates a default channel
struct link_Node *grp_createGroup(char *name, struct usr_UserData *user);

struct link_Node *grp_getGroup(char *name);

// Returns group's name safely
int grp_getName(struct link_Node *groupNode, char *buff, int size);

// Add user to the group and auto join to all public channels
struct grp_GroupUser *grp_addUser(struct link_Node *groupNode, struct usr_UserData *user, int permLevel);

struct grp_GroupUser *grp_isInGroup(struct link_Node *groupNode, struct usr_UserData *user);

struct link_Node *grp_addChannel(struct link_Node *groupNode, struct chan_Channel *chan);

struct link_Node *grp_getChannel(struct link_Node *groupNode, char *name);

int grp_isGroupMode(char mode);

// Fills string with names of users in the group
int grp_getUsersInGroup(struct link_Node *groupNode, char *buff, int size);

// Sends a message to all users in this group
int grp_sendGroupMessage(struct chat_Message *cmd, struct link_Node *groupNode);

#endif

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
	char modes[5];
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

// Add user to the group and auto join to all public channels
struct grp_GroupUser *grp_addUser(struct link_Node *groupNode, struct usr_UserData *user, int permLevel);

struct grp_GroupUser *grp_isInGroup(struct link_Node *groupNode, struct usr_UserData *user);

struct link_Node *grp_addChannel(struct link_Node *groupNode, struct chan_Channel *chan);

struct link_Node *grp_getChannel(struct link_Node *groupNode, char *name);

#endif

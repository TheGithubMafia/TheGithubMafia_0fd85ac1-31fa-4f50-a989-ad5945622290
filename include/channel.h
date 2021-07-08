#ifndef channel_h
#define channel_h

#include "user.h"
#include "chat.h"

#define CHANNEL_NAME_LENGTH 201

struct chat_Group;

// Data about a user specific to a channel
struct chan_ChannelUser {
	struct usr_UserData *user;
	int permLevel; // 0 - Default, 1 - chanvoice, 2 - chanop
};

struct chan_Channel {
	int id;
	int max;
	char modes[5];
	char name[CHANNEL_NAME_LENGTH];
	char key[20];
	struct chan_ChannelUser *users;
	pthread_mutex_t channelMutex;
};

// Returns the privs the user has for a channel
int chan_getUserChannelPrivs(struct usr_UserData *user, struct link_Node *chan);

int chan_removeUserFromChannel(struct link_Node *channelNode, struct usr_UserData *user);

int chan_removeUserFromAllChannels(struct usr_UserData *user);

// Returns the node to a channel if it exists
struct link_Node *chan_getChannelByName(char *name);

// Create a channel with the specified name, and add it to the specified group
struct link_Node *chan_createChannel(char *name, struct chat_Group *group);

int chan_channelHasMode(char mode, struct link_Node *channelNode);

// Take a channel mode and execute it
char *chan_executeChanMode(char op, char mode, struct link_Node *channel, char *data);

// Adds or removes a mode from a channel's modes array
void chan_changeChannelModeArray(char op, char mode, struct link_Node *channelNode);

int chan_isChanMode(char mode);

// Give or remove chan op or voice
char *chan_giveChanPerms(struct link_Node *channelNode, struct usr_UserData *user, char op, int perm);

// check if a user is in a channel
struct chan_ChannelUser *chan_isInChannel(struct link_Node *channelNode, struct usr_UserData *user);

// Places a pointer to the user into the Channel's list, and create it if needed
struct chan_ChannelUser *chan_addToChannel(struct link_Node *channelNode, struct usr_UserData *user);

// Will fill a buffer with list of nicknames
int chan_getUsersInChannel(struct link_Node *channelNode, char *buff, int size);

// Sends a message to all online users in this room
int chan_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode);

#endif

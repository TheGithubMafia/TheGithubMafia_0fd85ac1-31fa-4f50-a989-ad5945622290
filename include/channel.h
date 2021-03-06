#ifndef channel_h
#define channel_h

#include "user.h"
#include "chat.h"
#include "security.h"
#include "group.h"

/*	CHANNEL NAME FORMAT:
	&<groupname>/#<channelname>

	if no group name is supplied then
	it is assumed to be:
	&General-Chat/#<channelname>
*/

struct chat_Group;

// Data about a user specific to a channel
struct chan_ChannelUser {
	struct usr_UserData *user;
	int permLevel; // 0 - Default, 1 - chanvoice, 2 - chanop, 3 - groupop
};

struct chan_Channel {
	int id;
	char *name;
	char modes[NUM_MODES];
	char key[20];
	int max;
	struct chan_ChannelUser *users;
	struct link_Node *group;
	pthread_mutex_t channelMutex;
};

// Returns the privs the user has for a channel
int chan_getUserChannelPrivs(struct usr_UserData *user, struct link_Node *chan);

int chan_removeUserFromChannel(struct link_Node *channelNode, struct usr_UserData *user);

int chan_removeUserFromAllChannels(struct usr_UserData *user);

// Returns the node to a channel if it exists
struct link_Node *chan_getChannelByName(char *name);

// Returns full channel name
int chan_getName(struct link_Node *channelNode, char *buff, int size);

// Create a channel with the specified name, and add it to the specified group
struct link_Node *chan_createChannel(char *name, struct link_Node *group, struct usr_UserData *user);

int chan_channelHasMode(char mode, struct link_Node *channelNode);

// Take a channel mode and execute it
// Index is used to help the command parser know which parameter to use
char *chan_executeChanMode(char op, char mode, struct link_Node *channel, char *data, int *index);

// Adds or removes a mode from a channel's modes array
void chan_changeChannelModeArray(char op, char mode, struct link_Node *channelNode);

int chan_isChanMode(char mode);

// Sets the channel's key
char *chan_setKey(struct link_Node *channelNode, char *key);

// Removes the channel's key if it is correct
char *chan_removeKey(struct link_Node *channelNode, char *key);

// Checks to see if a given key is equal to the current key
int chan_checkKey(struct link_Node *channelNode, char *key);

// Give or remove chan op or voice
char *chan_giveChanPerms(struct link_Node *channelNode, struct usr_UserData *user, char op, int perm);

// check if a user is in a channel
struct chan_ChannelUser *chan_isInChannel(struct link_Node *channelNode, struct usr_UserData *user);

// Places a pointer to the user into the Channel's list
struct chan_ChannelUser *chan_addToChannel(struct link_Node *channelNode, struct usr_UserData *user, int permLevel);

// Will fill a buffer with list of nicknames
int chan_getUsersInChannel(struct link_Node *channelNode, char *buff, int size);

// Sends a message to all online users in this room
int chan_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode);

#endif

#include "channel.h"

const char chan_chanModes[] = {'o', 's', 'i', 'b', 'v', 'm', 'k'};

int chan_getUserChannelPrivs(struct usr_UserData *user, struct link_Node *chan) {
	if(chan == NULL || chan->data == NULL || user == NULL || user->id < 0){
		return -1;
	}

	struct chan_Channel *channel = chan->data;
	int ret = -1;

    pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
		if(channel->users[i].user == user){
			ret = channel->users[i].permLevel;
			break;
		}
	}
    pthread_mutex_unlock(&channel->channelMutex);

	return ret;
}

int chan_removeUserFromChannel(struct link_Node *channelNode, struct usr_UserData *user){
    struct chan_Channel *channel = channelNode->data;
    int ret = -1;

    if(channel == NULL || user == NULL){
        log_logMessage("Cannot remove user from channel", DEBUG);
        return -1;
    }

	pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
		if(channel->users[i].user == user){
			// Match
			memset(&channel->users[i], 0, sizeof(struct chan_ChannelUser));
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&channel->channelMutex);
    
    return ret;
}

int chan_removeUserFromAllChannels(UNUSED(struct usr_UserData *user)){
    int ret = -1;
/*
    struct link_Node *node;
    pthread_mutex_lock(&serverLists.channelsMutex);

    for(node = serverLists.channels.head; node != NULL; node = node->next){
            int num = chan_removeUserFromChannel(node, user);

            ret = ret == -1 ? num : ret;
    }

    pthread_mutex_unlock(&serverLists.channelsMutex);
	*/
    
    return ret;
}

// Use full name to get channel
struct link_Node *chan_getChannelByName(char *name){
	struct link_Node *group;

	char data[2][1000] = {0};
	int ret = chat_divideChanName(name, strlen(name), data);
	if(ret == -1)
		return NULL;

	if(data[0][0] == '\0'){
		group = serverLists.groups.head;
	} else {
		group = grp_getGroup(data[0]);
	}

	return grp_getChannel(group, data[1]);
}

// Returns full channel name
int chan_getName(struct link_Node *channelNode, char *buff, int size){
	struct chan_Channel *channel = channelNode->data;
	if(channel == NULL)
		return -1;

	struct grp_Group *group = channel->group->data;
	if(group == NULL)
		return -1;

	pthread_mutex_lock(&channel->channelMutex);
	pthread_mutex_lock(&group->groupMutex);
	snprintf(buff, size, "%s/%s", group->name, channel->name);
	pthread_mutex_unlock(&group->groupMutex);
	pthread_mutex_unlock(&channel->channelMutex);

	return 1;
}

// Create a channel with the specified name
// TODO - error checking with link_List
struct link_Node *chan_createChannel(char *name, struct link_Node *group, struct usr_UserData *user){
    if(name[0] != '#'){
        return NULL;
    }

    struct chan_Channel *channel;
    channel = calloc(1, sizeof(struct chan_Channel));
    if(channel == NULL){
        log_logError("Error creating channel", ERROR);
        return NULL;
    }
	channel->name = calloc(fig_Configuration.chanNameLength, sizeof(char));
    if(channel->name == NULL){
        log_logError("Error creating channel", ERROR);
		free(channel);
        return NULL;
    }

    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&channel->channelMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
		free(channel->name);
		free(channel);
        return NULL;
    }

    // TODO - make sure name is legal
    strncpy(channel->name, name, fig_Configuration.chanNameLength-1);

	// Setup array of users with default size of 10
	channel->max = 100;
	channel->users = calloc(channel->max, sizeof(struct chan_Channel));
	if(channel->users == NULL){
        log_logError("Error creating channel", ERROR);
		free(channel->name);
		free(channel);
        return NULL;
	}

    // Add to the group
	if(group == NULL)
		group = serverLists.groups.head; // Default group

	channel->group = group;
	struct link_Node *chanNode = grp_addChannel(group, channel);

	if(user != NULL){ // Add first user
		chan_addToChannel(chanNode, user, 2);
	}

	return chanNode;
}

int chan_channelHasMode(char mode, struct link_Node *channelNode){
	struct chan_Channel *channel = channelNode->data;
	int ret = -1;

	if(channelNode == NULL || channel == NULL){
		return -1;
	}

	pthread_mutex_lock(&channel->channelMutex);
	for (int i = 0; i < ARRAY_SIZE(channel->modes); i++){
		if(channel->modes[i] == mode) {
			ret = 1;
			break;
		}
	}
	pthread_mutex_unlock(&channel->channelMutex);

	return ret;
}

// Takes a channel mode and executes it
char *chan_executeChanMode(char op, char mode, struct link_Node *channelNode, char *data){
	int perm = 1;
	struct chan_Channel *channel = channelNode->data;
	struct usr_UserData *user;

	if(channelNode == NULL || channel == NULL){
		return ERR_NOSUCHCHANNEL;
	}

	switch (mode) {
		case 'o':
			perm++;
			goto change_chan_perm; // Fallthrough because 'v' and 'o' are only slightly different
		case 'v':
		change_chan_perm:
			user = usr_getUserByName(data);
			if(user == NULL){
				return ERR_NOSUCHNICK;
			}
			return chan_giveChanPerms(channelNode, user, op, perm);	

		case 'k':
			if(op == '+')
				return chan_setKey(channelNode, data);
			return chan_removeKey(channelNode, data);
		
		default: // No special action needed, simply add it to the array
			chan_changeChannelModeArray(op, mode, channelNode);
	}

	return NULL; // Successfull - no error message
}

// Adds or removes a mode from a channel's modes array
void chan_changeChannelModeArray(char op, char mode, struct link_Node *channelNode){
	struct chan_Channel *channel = channelNode->data;

	if(channelNode == NULL || channel == NULL)
		return;

	// Guarantee that mode only appears once
	if(op == '+' && chan_channelHasMode(mode, channelNode) == 1)
		return;

	pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < ARRAY_SIZE(channel->modes); i++){
		if(op == '+'){
			if(channel->modes[i] == '\0'){
				channel->modes[i] = mode;
				break;
			}
		} else {
			if(channel->modes[i] == mode){ 
				channel->modes[i] = '\0';
				break;
			}
		}
	}
	pthread_mutex_unlock(&channel->channelMutex);
}

int chan_isChanMode(char mode){
	for(int i = 0; i < ARRAY_SIZE(chan_chanModes); i++){
		if(mode == chan_chanModes[i]){
			return 1;
		}
	}
	
	return -1;
}

// Sets the channel's key
char *chan_setKey(struct link_Node *channelNode, char *key){
	struct chan_Channel *channel = channelNode->data;
	if(channelNode == NULL || channel == NULL)
		return ERR_UNKNOWNERROR;

	if(key == NULL)
		return ERR_NEEDMOREPARAMS;

	if(chan_channelHasMode('k', channelNode) == 1)
		return ERR_KEYSET;

	pthread_mutex_lock(&channel->channelMutex);
	strncpy(channel->key, key, ARRAY_SIZE(channel->key)-1);
	pthread_mutex_unlock(&channel->channelMutex);

	chan_changeChannelModeArray('+', 'k', channelNode);

	return NULL;
}

char *chan_removeKey(struct link_Node *channelNode, char *key){
	struct chan_Channel *channel = channelNode->data;
	if(channelNode == NULL || channel == NULL)
		return ERR_UNKNOWNERROR;

	if(key == NULL)
		return ERR_NEEDMOREPARAMS;

	// If keys match, remove channel's key
	pthread_mutex_lock(&channel->channelMutex);
	if(sec_constantStrCmp(channel->key, key, ARRAY_SIZE(channel->key)-1) == 1){
		channel->key[0] = '\0';
	} else {
		pthread_mutex_unlock(&channel->channelMutex);
		return ERR_BADCHANNELKEY;
	}
	pthread_mutex_unlock(&channel->channelMutex);

	chan_changeChannelModeArray('-', 'k', channelNode);

	return NULL;
}

// Checks if key matches, if no key then automatically correct
int chan_checkKey(struct link_Node *channelNode, char *key){
	struct chan_Channel *channel = channelNode->data;
	int ret = -1;
	if(channelNode == NULL || channel == NULL)
		return -1;

	// No need to check if there is no key to validate
	if(chan_channelHasMode('k', channelNode) == -1)
		return 1;

	if(key == NULL)
		return -1;

	// If keys match, remove channel's key
	pthread_mutex_lock(&channel->channelMutex);
	if(sec_constantStrCmp(channel->key, key, ARRAY_SIZE(channel->key)-1) == 1){
		ret = 1;
	}
	pthread_mutex_unlock(&channel->channelMutex);

	return ret;
}

// Remove or give chan op or voice
char *chan_giveChanPerms(struct link_Node *channelNode, struct usr_UserData *user, char op, int perm){
	if(channelNode == NULL || channelNode->data == NULL){
		return ERR_UNKNOWNERROR;
	}

    struct chan_Channel *channel = channelNode->data;
	struct chan_ChannelUser *chanUser = chan_isInChannel(channelNode, user);
	if(chanUser == NULL){
		return ERR_USERNOTINCHANNEL;
	}

    pthread_mutex_lock(&channel->channelMutex);
	if(op == '-'){
		chanUser->permLevel = 0;
	} else {
		chanUser->permLevel = perm;
	}
    pthread_mutex_unlock(&channel->channelMutex);

	return NULL;
}

// Check if a user is in a channel
struct chan_ChannelUser *chan_isInChannel(struct link_Node *channelNode, struct usr_UserData *user){
	if(user == NULL || channelNode == NULL || channelNode->data == NULL){
		return NULL;
	}

    struct chan_Channel *channel = channelNode->data;
	struct chan_ChannelUser *ret = NULL;

    pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
		if(channel->users[i].user == user){
            ret = &channel->users[i];
			break;
		}
	}
    pthread_mutex_unlock(&channel->channelMutex);

    return ret;
}

// Add a user to a channel
struct chan_ChannelUser *chan_addToChannel(struct link_Node *channelNode, struct usr_UserData *user, int permLevel){
    struct chan_Channel *channel = channelNode->data;
	struct chan_ChannelUser *chanUser = chan_isInChannel(channelNode, user);

    if(chanUser == NULL){ // Not in the channel
        pthread_mutex_lock(&channel->channelMutex);
		for(int i = 0; i < channel->max; i++){
			if(channel->users[i].user == NULL){ // Empty spot
				channel->users[i].user = user;
				channel->users[i].permLevel = permLevel;
				chanUser = &channel->users[i];
				break;
			}
		}
        pthread_mutex_unlock(&channel->channelMutex);
    }

    return chanUser;
}

// Will fill a string with a list of users
int chan_getUsersInChannel(struct link_Node *channelNode, char *buff, int size){
    struct chan_Channel *channel = channelNode->data;
    char nickname[fig_Configuration.nickLen];
    int pos = 1;

	strncpy(buff, ":", size);
    pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
		if(channel->users[i].user != NULL){
			switch(channel->users[i].permLevel) {
				case 2: // Channel operator
					strncat(buff, "@", size - pos - 1);
					pos++;
					break;

				case 1: // Channel voice
					strncat(buff, "+", size - pos - 1);
					pos++;
					break;
			}

			usr_getNickname(nickname, channel->users[i].user);
			strncat(buff, nickname, size - pos - 1);
			pos = strlen(buff);

			// Space inbetween users
			buff[pos] = ' ';
			buff[pos + 1] = '\0';
			pos++;
		}
    }
    pthread_mutex_unlock(&channel->channelMutex);

    return 1;
}

// Send a message to every user in a channel
int chan_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode){
	struct usr_UserData *origin = cmd->user;
    struct chan_Channel *channel = channelNode->data;

    pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
        cmd->user = channel->users[i].user;
		
		// Dont send to sender or invalid users
		if(cmd->user == origin || cmd->user == NULL){
			continue;
		}

        chat_sendMessage(cmd);
    }
    pthread_mutex_unlock(&channel->channelMutex);

	cmd->user = origin;

    return 1;
}


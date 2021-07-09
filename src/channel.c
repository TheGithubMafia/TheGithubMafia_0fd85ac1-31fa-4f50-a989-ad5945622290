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

    if(channel == NULL || user == NULL || user->id < 0){
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

int chan_removeUserFromAllChannels(struct usr_UserData *user){
    struct link_Node *node;
    int ret = -1;

    pthread_mutex_lock(&serverLists.channelsMutex);

    for(node = serverLists.channels.head; node != NULL; node = node->next){
            int num = chan_removeUserFromChannel(node, user);

            ret = ret == -1 ? num : ret;
    }

    pthread_mutex_unlock(&serverLists.channelsMutex);
    
    return ret;
}

struct link_Node *chan_getChannelByName(char *name){
    struct link_Node *node;
    struct chan_Channel *channel;

    if(name[0] != '#'){
        return NULL;
    }

    pthread_mutex_lock(&serverLists.channelsMutex);

    for(node = serverLists.channels.head; node != NULL; node = node->next){
            channel = node->data;
            pthread_mutex_lock(&channel->channelMutex);

            if(!strncmp(channel->name, name, CHANNEL_NAME_LENGTH)){
                    pthread_mutex_unlock(&channel->channelMutex);
                    pthread_mutex_unlock(&serverLists.channelsMutex);
                    return node;
            }

            pthread_mutex_unlock(&channel->channelMutex);
    }

    pthread_mutex_unlock(&serverLists.channelsMutex);

    return NULL;
}

// Create a channel with the specified name
// TODO - add further channel properties
// TODO - error checking with link_List
struct link_Node *chan_createChannel(char *name, struct chat_Group *group){
    if(name[0] != '#'){
        return NULL;
    }

    struct chan_Channel *channel;
    channel = malloc(sizeof(struct chan_Channel));
    if(channel == NULL){
        log_logError("Error creating channel", ERROR);
        return NULL;
    }

    // TODO - make sure name is legal
    strncpy(channel->name, name, CHANNEL_NAME_LENGTH);

	// Setup array of users with default size of 10
	channel->max = 10;
	channel->users = calloc(sizeof(struct chan_Channel), channel->max);
	if(channel->users == NULL){
        log_logError("Error creating channel", ERROR);
        return NULL;
	}

    // Add to the group
    if(group != NULL){
        pthread_mutex_lock(&group->groupMutex);
        link_add(&group->channels, channel);
        pthread_mutex_unlock(&group->groupMutex);
    }

    pthread_mutex_lock(&serverLists.channelsMutex);
    struct link_Node *node = link_add(&serverLists.channels, channel);
    pthread_mutex_unlock(&serverLists.channelsMutex);

    return node;
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
		
		default: // No special action needed, simply add it to the array
			chan_changeChannelModeArray(op, mode, channelNode);
	}

	return NULL; // Successfull - no error message
}

// Adds or removes a mode from a channel's modes array
// TODO - adding/removing could be more efficient
void chan_changeChannelModeArray(char op, char mode, struct link_Node *channelNode){
	struct chan_Channel *channel = channelNode->data;

	if(channelNode == NULL || channel == NULL)
		return;

	pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < ARRAY_SIZE(channel->modes); i++){
		if(op == '+'){
			if(channel->modes[i] == mode){
				op = '-';
			}

			if(channel->modes[i] == '\0'){
				channel->modes[i] = mode;
				op = '-';
			}
		} else {
			if(channel->modes[i] == mode){ // Go thru to remove all duplicates
				channel->modes[i] = '\0';
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
struct chan_ChannelUser *chan_addToChannel(struct link_Node *channelNode, struct usr_UserData *user){
    struct chan_Channel *channel = channelNode->data;
	struct chan_ChannelUser *chanUser = NULL;

    if(chan_isInChannel(channelNode, user) == NULL){ // Not in the channel // Not in the channel
        pthread_mutex_lock(&channel->channelMutex);
		for(int i = 0; i < channel->max; i++){
			if(channel->users[i].user == NULL){ // Empty spot
				channel->users[i].user = user;
				channel->users[i].permLevel = 0;
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

    buff[0] = ':';
    pthread_mutex_lock(&channel->channelMutex);
	for(int i = 0; i < channel->max; i++){
		if(channel->users[i].user != NULL){
			if(channel->users[i].permLevel == 2){ // Channel operator
				strncat(buff, "@", size - pos - 1);
				pos++;
			} else if (channel->users[i].permLevel == 1){ // Channel voice
				strncat(buff, "+", size - pos - 1);
				pos++;
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


#include "group.h"

const char grp_groupModes[] = {'o', 's', 'i', 'b', 'm', 'k'};

// Creates a new group, adds it to the main list, and creates a default channel
struct link_Node *grp_createGroup(char *name, struct usr_UserData *user){
	if(name[0] != '&'){
		return NULL;
	}

	struct grp_Group *group;
	group = calloc(1, sizeof(struct grp_Group));
	if(group == NULL){
		log_logError("Error allocating group", WARNING);
		return NULL;
	}
	group->name = calloc(1, fig_Configuration.groupNameLength);
	if(group->name == NULL){
		log_logError("Error allocating group", WARNING);
		free(group);
		return NULL;
	}

    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&group->groupMutex, NULL);
    if (ret < 0){
		free(group->name);
		free(group);
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return NULL;
    }

	strncpy(group->name, name, fig_Configuration.groupNameLength-1);

	group->max = 100;
	group->users = calloc(group->max, sizeof(struct grp_GroupUser));
	if(group->users == NULL){
        log_logError("Error creating group", ERROR);
		free(group->name);
		free(group);
        return NULL;
	}

	// Add to main list
    pthread_mutex_lock(&serverLists.groupsMutex);
    struct link_Node *node = link_add(&serverLists.groups, group);
    pthread_mutex_unlock(&serverLists.groupsMutex);

	if(node == NULL){
		free(group->name);
		free(group->users);
		free(group);
        return NULL;
	}

	// Generate a default Channel
	chan_createChannel("#default", node, user);

	// Make selected user OPER of the group
	grp_addUser(node, user, 1);

	return node;
}

struct link_Node *grp_getGroup(char *name){
	struct link_Node *node;
	struct grp_Group *group;

	if(name[0] == '\0') // Null defaults to default group
		return serverLists.groups.head;

	if(name[0] != '&')
		return NULL;

	pthread_mutex_lock(&serverLists.groupsMutex);
	
	for(node = serverLists.groups.head; node != NULL; node = node->next){
		group = node->data;
		pthread_mutex_lock(&group->groupMutex);

		if(!strncmp(group->name, name, fig_Configuration.groupNameLength)){
			pthread_mutex_unlock(&serverLists.groupsMutex);
			pthread_mutex_unlock(&group->groupMutex);
			return node;
		}

		pthread_mutex_unlock(&group->groupMutex);
	}

	pthread_mutex_unlock(&serverLists.groupsMutex);

	return NULL;
}

// Returns group's name safely
int grp_getName(struct link_Node *groupNode, char *buff, int size){
	if(groupNode == NULL || groupNode->data == NULL)
		return -1;

	struct grp_Group *group = groupNode->data;
	pthread_mutex_lock(&group->groupMutex);
	strncpy(buff, group->name, size);
	pthread_mutex_unlock(&group->groupMutex);

	return 1;
}

// Add user to the group and auto join to all public channels
// TODO -auto join channels
// TODO - send names message
// TODO check for key access
struct grp_GroupUser *grp_addUser(struct link_Node *groupNode, struct usr_UserData *user, int permLevel){
	struct grp_Group *group = groupNode->data;
	struct grp_GroupUser *grpUser = NULL;

	if(group == NULL)
		return NULL;

	// Already in group
	grpUser = grp_isInGroup(groupNode, user);
	if(grpUser != NULL)
		return grpUser;

	pthread_mutex_lock(&group->groupMutex);
	for(int i = 0; i < group->max; i++){
		if(group->users[i].user == NULL){
			group->users[i].user = user;
			group->users[i].permLevel = permLevel;
			grpUser = &group->users[i];
			break;
		}
	}
	pthread_mutex_unlock(&group->groupMutex);

	return grpUser;
}

struct grp_GroupUser *grp_isInGroup(struct link_Node *groupNode, struct usr_UserData *user){
	struct grp_Group *group = groupNode->data;
	struct grp_GroupUser *grpUser = NULL;

	if(group == NULL)
		return NULL;

	// Go thru each user and check
	pthread_mutex_lock(&group->groupMutex);
	for(int i = 0; i < group->max; i++){
		if(group->users[i].user == user){
			grpUser = &group->users[i];
			break;
		}
	}
	pthread_mutex_unlock(&group->groupMutex);
	
	return grpUser;
}

struct link_Node *grp_addChannel(struct link_Node *groupNode, struct chan_Channel *chan){
	if(groupNode == NULL || groupNode->data == NULL || chan == NULL)
		return NULL;
		
	struct grp_Group *group = groupNode->data;

	pthread_mutex_lock(&group->groupMutex);
	struct link_Node *chanNode = link_add(&group->channels, chan);
	pthread_mutex_unlock(&group->groupMutex);

	return chanNode;
}

struct link_Node *grp_getChannel(struct link_Node *groupNode, char *name){
	struct grp_Group *group = groupNode->data;
	if(group == NULL)
		return NULL;

	struct link_Node *node;
	struct chan_Channel *channel;
	pthread_mutex_lock(&group->groupMutex);
	
	for(node = group->channels.head; node != NULL; node = node->next){
		channel = node->data;
		pthread_mutex_lock(&channel->channelMutex);

		if(!strncmp(channel->name, name, fig_Configuration.chanNameLength)){
			pthread_mutex_unlock(&group->groupMutex);
			pthread_mutex_unlock(&channel->channelMutex);
			return node;
		}

		pthread_mutex_unlock(&channel->channelMutex);
	}

	pthread_mutex_unlock(&group->groupMutex);

	return NULL;
}

int grp_isGroupMode(char mode){
	for(int i = 0; i < ARRAY_SIZE(grp_groupModes); i++){
		if(mode == grp_groupModes[i]){
			return 1;
		}
	}
	
	return -1;
}

// Fills string with names of users in the group
int grp_getUsersInGroup(struct link_Node *groupNode, char *buff, int size){
    struct grp_Group *group = groupNode->data;
    char nickname[fig_Configuration.nickLen];
    int pos = 1;

    buff[0] = ':';
    pthread_mutex_lock(&group->groupMutex);
	for(int i = 0; i < group->max; i++){
		if(group->users[i].user != NULL){
			switch(group->users[i].permLevel) {
				case 1: // Group operator
					strncat(buff, "^", size - pos - 1);
					pos++;
					break;
			}

			usr_getNickname(nickname, group->users[i].user);
			strncat(buff, nickname, size - pos - 1);
			pos = strlen(buff);

			// Space inbetween users
			buff[pos] = ' ';
			buff[pos + 1] = '\0';
			pos++;
		}
    }
    pthread_mutex_unlock(&group->groupMutex);

    return 1;
}

int grp_sendGroupMessage(struct chat_Message *cmd, struct link_Node *groupNode){
	struct usr_UserData *origin = cmd->user;
    struct grp_Group *group = groupNode->data;

    pthread_mutex_lock(&group->groupMutex);
	for(int i = 0; i < group->max; i++){
        cmd->user = group->users[i].user;
		
		// Dont send to sender or invalid users
		if(cmd->user == origin || cmd->user == NULL){
			continue;
		}

        chat_sendMessage(cmd);
    }
    pthread_mutex_unlock(&group->groupMutex);

	cmd->user = origin;

    return 1;
}

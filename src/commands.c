#include "commands.h"

struct cmd_CommandList cmd_commandList;
struct chat_Message cmd_unknownCommand;
char *thisServer = "example.boundless.chat"; 

// Common reply messages
const char *invalidChanName = ":Invalid channel name";

int init_commands() {
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&cmd_commandList.commandMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex.", ERROR);
        return -1;
    }

	if(fig_Configuration.serverName[0] != '\0'){
		thisServer = fig_Configuration.serverName;
	}

    // Init cmd_unknownCommand
    char *params[] = {":Unknown command: "};
    chat_createMessage(&cmd_unknownCommand, NULL, thisServer, ERR_UNKNOWNCOMMAND, params, 1);
    cmd_unknownCommand.user = NULL;

	// Fill in command linked list
	//			  WORD PARAM PERM COMMAND  
    cmd_addCommand("NICK", 1, 0, &cmd_nick);
    cmd_addCommand("PRIVMSG", 2, 1, &cmd_privmsg);
    cmd_addCommand("JOIN", 1, 1, &cmd_join);
    cmd_addCommand("NAMES", 1, 1, &cmd_names);
    cmd_addCommand("PART", 1, 1, &cmd_part);
    cmd_addCommand("KICK", 2, 1, &cmd_kick);
    cmd_addCommand("MODE", 2, 1, &cmd_mode);
    cmd_addCommand("PING", 0, 0, &cmd_ping);
    cmd_addCommand("PONG", 0, 0, &cmd_pong);

    log_logMessage("Successfully initalized commands.", INFO);
    return 1;
}

int cmd_addCommand(char *word, int minParams, int permLevel, int (*func)(struct chat_Message *, struct chat_Message *)) {
    struct cmd_Command *command = malloc(sizeof(struct cmd_Command));
    if(command == NULL){
        log_logError("Failed to allocate cmd_Command!", DEBUG);
        return -1;
    }

    command->minParams = minParams;
    strncpy(command->word, word, ARRAY_SIZE(command->word));	
    command->func = func;
	command->permLevel = permLevel;

    pthread_mutex_lock(&cmd_commandList.commandMutex);
    link_add(&cmd_commandList.commands, command);
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    return 1;
}

int cmd_runCommand(struct chat_Message *cmd){
    struct link_Node *cmdNode;
    struct chat_Message reply;
    struct cmd_Command *command;
    int ret = -2;

    // Loop thru the commands looking for the same command
    pthread_mutex_lock(&cmd_commandList.commandMutex);
    for(cmdNode = cmd_commandList.commands.head; cmdNode != NULL; cmdNode = cmdNode->next){
        if(cmdNode->data == NULL){
            continue;
        }

        command = cmdNode->data;
        if(!strncmp(command->word, cmd->command, ARRAY_SIZE(command->word))){
            // Successful match
            pthread_mutex_unlock(&cmd_commandList.commandMutex);
            ret = -1; // Default to failure

			if(usr_userHasMode(cmd->user, 'r') == 1 && command->permLevel >= 1){
                char *params[] = {":You have not registered: use NICK first"};
                chat_createMessage(&reply, cmd->user, thisServer, ERR_NOTREGISTERED, params, 1);
                break;
			}

            // Check number of params
            if(cmd->paramCount < command->minParams){
                char *params[] = {":Command needs more params"};
                chat_createMessage(&reply, cmd->user, thisServer, ERR_NEEDMOREPARAMS, params, 1);
                break;
            }

            ret = command->func(cmd, &reply);
            break;
        }
    }
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    // Unknown command: -1 is reserved for known command error
    if(ret == -2){
        memcpy(&reply, &cmd_unknownCommand, sizeof(reply));
        reply.user = cmd->user;
        strncat(reply.params[0], cmd->command, 15);
    }

    if(ret != 2){ // 2 is a request that message is not sent
        chat_sendMessage(&reply);
    }
    return ret;
}

struct link_Node *cmd_checkChannelPerms(struct chat_Message *msg, char *chanName, struct usr_UserData *user, int reqPrivs) {
    struct link_Node *chan = chan_getChannelByName(chanName);
	char *params[] = {chanName};

    if(chan == NULL){
        params[1] = (char *) invalidChanName;
        chat_createMessage(msg, user, thisServer, ERR_NOSUCHCHANNEL, params, 2);
        return NULL;
    }

	if(chan_isInChannel(chan, user) == NULL){
		err_notonchannel(msg, chan->data, user);	
		return NULL;
	}

	if(chan_getUserChannelPrivs(user, chan) < reqPrivs) {
		err_chanoprivsneeded(msg, chan->data, user);
		return NULL;
	}

	return chan;
}

// Generate a ERR_NOTONCHANNEL reply
void err_notonchannel(struct chat_Message *msg, struct chan_Channel *chan, struct usr_UserData *user){
	char *params[] = {chan->name, ":You are not in this channel!"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, ERR_NOTONCHANNEL, params, 2);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a RPL_ENDOFNAMES reply
void rpl_endofnames(struct chat_Message *msg, struct chan_Channel *chan, struct usr_UserData *user){
	char *params[] = {chan->name, ":End of /NAMES list"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, RPL_ENDOFNAMES, params, 2);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a ERR_USERNOTINCHANNEL reply
void err_usernotinchannel(struct chat_Message *msg, struct chan_Channel *chan, struct usr_UserData *user, char *nick){
	char *params[] = {nick, chan->name, ":Selected user not in this channel!"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, ERR_USERNOTINCHANNEL, params, 3);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a ERR_CHANOPRIVSNEEDED reply
void err_chanoprivsneeded(struct chat_Message *msg, struct chan_Channel *chan, struct usr_UserData *user){
	char *params[] = {chan->name, ":You don't have sufficient privileges for this channel!"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, ERR_CHANOPRIVSNEEDED, params, 2);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Changes a user's nickname
const char *nick_usage = ":Usage: NICK <nickname>";
const char *nick_welcome = ":Welcome to the server!";
const char *nick_inUse = "Nickname already in use!";
int cmd_nick(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

    // No nickname given
    if(cmd->params[0][0] == '\0'){
		params[0] = (char *) nick_usage;
        chat_createMessage(reply, user, thisServer, ERR_NONICKNAMEGIVEN, params, 1);
        return 1;
    }

    struct usr_UserData *otherUser = usr_getUserByName(cmd->params[0]);
    if(otherUser == NULL) { // No other user has this name
		char oldName[fig_Configuration.nickLen];
		usr_getNickname(oldName, user);
		int isUnreg = usr_userHasMode(user, 'r');

		// Set the name in the user's buffer
        pthread_mutex_lock(&user->userMutex);
        strncpy(user->nickname, cmd->params[0], fig_Configuration.nickLen-1);
        pthread_mutex_unlock(&user->userMutex);

        params[0] = cmd->params[0];
		// User is already registered
		if(isUnreg != 1){
			chat_createMessage(reply, user, oldName, "NICK", params, 1);
			chat_sendServerMessage(reply); // TODO - change to all channels user is in + "contacts"
			return 1;
		}

		usr_changeUserMode(user, '-', 'r'); // They are now registered
		params[1] = (char *) nick_welcome;
		chat_createMessage(reply, user, thisServer, RPL_WELCOME, params, 2);
		return 1;

    }

	params[0] = cmd->params[0];
	params[1] = (char *) nick_inUse;

	chat_createMessage(reply, user, thisServer, ERR_NICKNAMEINUSE, params, 2);
	return 1;
}

// Send a message to user or channel
// TODO - add multiple receivers -> <receiver>{,<receiver>}
int cmd_privmsg(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    struct usr_UserData *otherUser;
    struct link_Node *channel;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;

    // Sending to another user
    params[0] = cmd->params[0];
    size = 2;
    if(cmd->params[0][0] != '#'){
        otherUser = usr_getUserByName(cmd->params[0]);
        if(otherUser == NULL){
			params[1] = ":Nick not found!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, size);
            return -1;
        }
    } else { // To a channel
		channel = cmd_checkChannelPerms(reply, cmd->params[0], user, 0);
        if(channel == NULL)
			return -1;

		if(chan_channelHasMode('m', channel) == 1 && chan_getUserChannelPrivs(user, channel) < 1){
			chat_createMessage(reply, user, thisServer, ERR_CANNOTSENDTOCHAN, params, 1);
			return -1;
		}
    }

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);

    params[0] = cmd->params[0];
    params[1] = cmd->params[1];

    chat_createMessage(reply, otherUser, nickname, "PRIVMSG", params, size);
    if(channel == NULL){
        return 1;
    }

	reply->user = user;
    chan_sendChannelMessage(reply, channel);
    return 2;
}

// Join a channel and/or a group
// TODO - key for access
// TODO - add error checking
const char *join_usage = ":Usage: <channel>";
int cmd_join(struct chat_Message *cmd, struct chat_Message *reply){
	struct usr_UserData *user = cmd->user;
	char *params[ARRAY_SIZE(cmd->params)];
	params[0] = cmd->params[0];
	params[1] = ":Invalid channel name format";

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);

	// Used to generate the NAMES command later
	char namesCMD[MAX_MESSAGE_LENGTH] = "NAMES "; 

	// Split name into group and channel
	char names[2][1000];
	int ret = chat_divideChanName(cmd->params[0], strlen(cmd->params[0]), names);
	if(ret == -1){
		chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 2);
		return -1;
	}

	// Sort out what to do regarding the group section
	struct link_Node *groupNode = grp_getGroup(names[0]);
	if(groupNode == NULL){ // Create a new group
		groupNode = grp_createGroup(names[0], user);

		if(groupNode == NULL){
			chat_createMessage(reply, user, thisServer, ERR_NOSUCHGROUP, params, 1);
			return -1;
		}

		// NAMES
		char buff[fig_Configuration.groupNameLength];
		grp_getName(groupNode, buff, ARRAY_SIZE(buff));
		strncat(namesCMD, buff, ARRAY_SIZE(namesCMD)-strlen(namesCMD)-2);
	} else { // Join if not already in
		if(grp_isInGroup(groupNode, user) == NULL){
			struct grp_GroupUser *grpUsr = grp_addUser(groupNode, user, 0);
			if(grpUsr == NULL){
				// FULL
				chat_createMessage(reply, user, thisServer, ERR_GROUPISFULL, params, 1);
				return -1;
			}

			char buff[fig_Configuration.groupNameLength];
			grp_getName(groupNode, buff, ARRAY_SIZE(buff));

			params[0] = buff;
			chat_createMessage(reply, user, nick, "JOIN", params, 1);
			grp_sendGroupMessage(reply, groupNode);
			params[0] = cmd->params[0]; // Reset back to default

			// NAMES
			strncat(namesCMD, buff, ARRAY_SIZE(namesCMD)-strlen(namesCMD)-2);
		}
	}

	if(names[1][0] == '\0'){ // No further action: group joined
		// Generate names for the JOIN
		chat_insertQueue(user, 0, namesCMD, NULL);
		return 2;
	}

	// Sort out channel	
	struct link_Node *channelNode = grp_getChannel(groupNode, names[1]);
	if(channelNode == NULL){ // Create it
		channelNode = chan_createChannel(names[1], groupNode, user);

		if(channelNode == NULL){ // Still a problem
			chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
			return -1;
		}

		if(cmd->paramCount > 1)
			chan_setKey(channelNode, cmd->params[1]);
	} else {
		if(chan_checkKey(channelNode, cmd->params[1]) == -1){ // Invalid key
			params[1] = cmd->params[0];
			params[0] = nick;
			chat_createMessage(reply, user, thisServer, ERR_BADCHANNELKEY, params, 2);
			return -1;
		}
		
		struct chan_ChannelUser *chanUsr = chan_addToChannel(channelNode, user, 0);
		if(chanUsr == NULL){
			// FULL
			chat_createMessage(reply, user, thisServer, ERR_CHANNELISFULL, params, 1);
			return -1;
		}

		chat_createMessage(reply, user, nick, "JOIN", params, 1);
		chan_sendChannelMessage(reply, channelNode);
	}

	// Generate names for the JOIN
	if(namesCMD[6] != '\0') // Also joined a GROUP
		strcat(namesCMD, ",");
	strncat(namesCMD, cmd->params[0], ARRAY_SIZE(namesCMD)-strlen(namesCMD)-2);
	chat_insertQueue(user, 0, namesCMD, NULL);

	return 2;
}

// Returns list of names
// TODO - Hidden/private channels and multiple channels
int cmd_names(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	char items[5][1001] = {0};

	char buff[BUFSIZ];
	strncpy(buff, cmd->params[0], ARRAY_SIZE(buff));
    
	// Split up into array based on commas
	int loc = 0, num = 0;
	while(loc != -1 && num < ARRAY_SIZE(items)){
		int oldLoc = loc;
		loc = chat_findCharacter(buff, strlen(cmd->params[0]), ',');
		if(loc > -1){
			buff[loc] = '\0'; // Aid to strncpy	
			loc++; // Start at char after ','
		}
		strncpy(items[num], &buff[oldLoc], ARRAY_SIZE(items[0])-1);

		num++;
	}

	char nick[fig_Configuration.nickLen];
	usr_getNickname(nick, user);
	char names[ARRAY_SIZE(cmd->params[0])];
	for(int i = 0; i < ARRAY_SIZE(items); i++){
		if(items[i][0] == '\0')
			break;
		
		// Default structure
		params[0] = nick;
		params[1] = "=";
		params[2] = items[i];
		params[3] = names;

		// Check to see if channel is the correct one first
		struct link_Node *chan = chan_getChannelByName(items[i]);
		if(chan == NULL && items[i][0] != '&'){ // Both invalid
			params[0] = items[i];
			chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
			continue;
		} else if(chan != NULL){ // Channel valid
			chan_getUsersInChannel(chan, names, ARRAY_SIZE(names));
		} else { // Group valid
			struct link_Node *group = grp_getGroup(items[i]);
			if(group == NULL){
				params[0] = items[i];
				chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
				continue;
			}

			grp_getUsersInGroup(group, names, ARRAY_SIZE(names));
		}

		chat_createMessage(reply, user, thisServer, RPL_NAMREPLY, params, 4);
		chat_sendMessage(reply);
	}

	params[0] = cmd->params[0];
	params[1] = ":End of /NAMES list";
	chat_createMessage(reply, user, thisServer, RPL_ENDOFNAMES, params, 2);
	return 1;
}

// Leave a channel or group
// TODO - add error checking
int cmd_part(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;

    struct link_Node *channel = chan_getChannelByName(cmd->params[0]);
    if(cmd->params[0][0] != '#' || channel == NULL){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
        return -1;
    }

    if(chan_removeUserFromChannel(channel, user) < 0) { // Not in the channel
		err_notonchannel(reply, channel->data, user);
		return 1;
	}

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);
    params[0] = cmd->params[0];
    size = 1;

    chat_createMessage(reply, user, nickname, "PART", params, size);
    chan_sendChannelMessage(reply, channel);

    return 1;
}

// Force a user to leave a channel
// TODO - add error checking
int cmd_kick(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user, *otherUser;
    char *params[ARRAY_SIZE(cmd->params)];
	int size = 2;

    struct link_Node *channel = cmd_checkChannelPerms(reply, cmd->params[0], user, 2);
	if(!channel){
		return -1;
	}

    otherUser = usr_getUserByName(cmd->params[1]);
    if(chan_removeUserFromChannel(channel, otherUser) < 0) { // Not in the channel
		err_usernotinchannel(reply, channel->data, user, cmd->params[1]);
		return -1;
	}

    // Success
    char nickname[fig_Configuration.nickLen];
    usr_getNickname(nickname, user);
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	if(cmd->params[2][0] != '\0'){
		params[2] = cmd->params[2];
		size = 3;
	}

    chat_createMessage(reply, otherUser, nickname, "KICK", params, size);
    chan_sendChannelMessage(reply, channel);

    return 1;
}

// Edit modes for channels, groups and users
int cmd_mode(struct chat_Message *cmd, struct chat_Message *reply){
	struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	int type = TYPE_USER;
    if(chan_getChannelByName(cmd->params[0]) != NULL){
		type = TYPE_CHAN;
	} else if(cmd->params[0][0] == '&') { // Last possible option is a group
		type = TYPE_GROUP;
	}

	// Make sure all modes are valid
	char op = cmd->params[1][0];
	int hasOp = op == '-' || op == '+' ? 1 : 0; // If there is an operation dont check it
	if(hasOp == 0){
		op = '+'; // If no op given, default to +
	}

	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		if(chat_isValidMode(cmd->params[1][i], type) == -1){
			char rpl[20];
			snprintf(rpl, ARRAY_SIZE(rpl), ":No such mode: %c", cmd->params[1][i]);

			params[1] = rpl;
			chat_createMessage(reply, user, thisServer, ERR_UNKNOWNMODE, params, 2);
			return -1;
		}

		// May not set themselves as OP or registered or away
		if(type == TYPE_USER && (cmd->params[1][i] == 'o' || cmd->params[1][i] == 'r' || cmd->params[1][i] == 'a')){
			return 2; // Say nothing
		}
	}

	//Check whether mode is for channel or user
	switch(type){
		case TYPE_CHAN:
			return cmd_modeChan(cmd, reply, op, hasOp);
			
		case TYPE_GROUP:
			//return cmd_modeGroup(cmd, reply, op, hasOp);
			return 2;
	}

	return cmd_modeUser(cmd, reply, op, hasOp);
}

// Used by cmd_mode specifically for the user
int cmd_modeUser(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct usr_UserData *user = cmd->user, *otherUser = NULL;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	otherUser = usr_getUserByName(cmd->params[0]);
	if(otherUser == NULL){
		params[1] = ":Nick not found!";
		chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, 2);
		return -1;
	} else if (otherUser != user){
		params[1] = ":You may not MODE a user other than yourself";
		chat_createMessage(reply, user, thisServer, ERR_USERSDONTMATCH, params, 2);
		return -1;
	}

	// Set all the modes
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		usr_changeUserMode(user, op, cmd->params[1][i]);		
	}

	chat_createMessage(reply, user, cmd->params[0], "MODE", params, 2);
	return 1;
}

// Used by cmd_mode specifically for a channel
int cmd_modeChan(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct usr_UserData *user = cmd->user;
	struct link_Node *channel = NULL;
    char *params[ARRAY_SIZE(cmd->params)];
	char nickname[fig_Configuration.nickLen];
	usr_getNickname(nickname, user);

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	params[2] = cmd->params[2];

	channel = cmd_checkChannelPerms(reply, cmd->params[0], user, 2);
	if(!channel){
		return -1;
	}

	// Set all the modes
	int index = 0;
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		params[2+index] = cmd->params[2+index]; // Fill in extra parameters
		char *ret = chan_executeChanMode(op, cmd->params[1][i], channel, cmd->params[2+index], &index);		
		if(ret != NULL){
			params[0] = nickname;
			params[1] = params[2]; // Problematic value
			chat_createMessage(reply, user, thisServer, ret, params, 1);
			return -1;
		}
	}

	chat_createMessage(reply, NULL, nickname, "MODE", params, 3+index);
    chan_sendChannelMessage(reply, channel);
	return 2;
}

// Used by cmd_mode specifically for a group
/*
int cmd_modeGroup(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct usr_UserData *user = cmd->user;
	struct link_Node *group = NULL;
    char *params[ARRAY_SIZE(cmd->params)];
	char nickname[fig_Configuration.nickLen];
	usr_getNickname(nickname, user);

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	params[2] = cmd->params[2];

	group = cmd_checkChannelPerms(reply, cmd->params[0], user, 2);
	if(!channel){
		return -1;
	}

	// Set all the modes
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		//TODO - multiple data for multiple flags
		char *ret = chan_executeChanMode(op, cmd->params[1][i], channel, cmd->params[2]);		
		if(ret != NULL){
			params[0] = nickname;
			params[1] = params[2]; // Problematic value
			chat_createMessage(reply, user, thisServer, ret, params, 1);
			return -1;
		}
	}

	chat_createMessage(reply, NULL, nickname, "MODE", params, 3);
    chan_sendChannelMessage(reply, channel);
	return 2;
}
*/

// Send back a PONG
int cmd_ping(struct chat_Message *cmd, struct chat_Message *reply){
    struct usr_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	int size = 0;

	if(cmd->paramCount > 0){
		params[0] = cmd->params[0];
		size = 1;
	}

	chat_createMessage(reply, user, NULL, "PONG", params, size);
	return 1;
}

// Response to PONG = do nothing
int cmd_pong(UNUSED(struct chat_Message *cmd), UNUSED(struct chat_Message *reply)){
	return 2;
}

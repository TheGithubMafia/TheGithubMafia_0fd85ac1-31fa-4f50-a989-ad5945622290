#include "commands.h"

struct cmd_CommandList cmd_commandList;
struct chat_Message cmd_unknownCommand;
char *thisServer = "roundtable.example.com"; 

// Common reply messages
const char *invalidChanName = ":Invalid channel name";

int init_commands() {
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&cmd_commandList.commandMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex", ERROR);
        return -1;
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

    log_logMessage("Successfully initalized commands", INFO);
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

			if(chat_userHasMode(cmd->user, 'r') == 1 && command->permLevel >= 1){
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

struct link_Node *cmd_checkChannelPerms(struct chat_Message *msg, char *chanName, struct chat_UserData *user, int reqPrivs) {
    struct link_Node *chan = chat_getChannelByName(chanName);
	char *params[] = {chanName};

    if(chan == NULL){
        params[1] = (char *) invalidChanName;
        chat_createMessage(msg, user, thisServer, ERR_NOSUCHCHANNEL, params, 2);
        return NULL;
    }

	if(chat_isInChannel(chan, user) == NULL){
		err_notonchannel(msg, chan->data, user);	
		return NULL;
	}

	if(chat_getUserChannelPrivs(user, chan) < reqPrivs) {
		err_chanoprivsneeded(msg, chan->data, user);
		return NULL;
	}

	return chan;
}

// Generate a ERR_NOTONCHANNEL reply
void err_notonchannel(struct chat_Message *msg, struct chat_Channel *chan, struct chat_UserData *user){
	char *params[] = {chan->name, ":You are not in this channel!"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, ERR_NOTONCHANNEL, params, 2);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a RPL_ENDOFNAMES reply
void rpl_endofnames(struct chat_Message *msg, struct chat_Channel *chan, struct chat_UserData *user){
	char *params[] = {chan->name, ":End of /NAMES list"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, RPL_ENDOFNAMES, params, 2);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a ERR_USERNOTINCHANNEL reply
void err_usernotinchannel(struct chat_Message *msg, struct chat_Channel *chan, struct chat_UserData *user, char *nick){
	char *params[] = {nick, chan->name, ":Selected user not in this channel!"};
	pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
	chat_createMessage(msg, user, thisServer, ERR_USERNOTINCHANNEL, params, 3);
	pthread_mutex_unlock(&chan->channelMutex);
}

// Generate a ERR_CHANOPRIVSNEEDED reply
void err_chanoprivsneeded(struct chat_Message *msg, struct chat_Channel *chan, struct chat_UserData *user){
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
    struct chat_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

    // No nickname given
    if(cmd->params[0][0] == '\0'){
		params[0] = (char *) nick_usage;
        chat_createMessage(reply, user, thisServer, ERR_NONICKNAMEGIVEN, params, 1);
        return 1;
    }

    struct chat_UserData *otherUser = chat_getUserByName(cmd->params[0]);
    if(otherUser == NULL) { // No other user has this name
		char oldName[NICKNAME_LENGTH];
		chat_getNickname(oldName, user);
		int isUnreg = chat_userHasMode(user, 'r');

		// Set the name in the user's buffer
        pthread_mutex_lock(&user->userMutex);
        strncpy(user->nickname, cmd->params[0], NICKNAME_LENGTH);
        pthread_mutex_unlock(&user->userMutex);

        params[0] = cmd->params[0];
		// User is already registered
		if(isUnreg != 1){
			chat_createMessage(reply, user, oldName, "NICK", params, 1);
			chat_sendServerMessage(reply); // TODO - change to all channels user is in + "contacts"
			return 1;
		}

		chat_changeUserMode(user, '-', 'r'); // They are now registered
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
    struct chat_UserData *user = cmd->user;
    struct chat_UserData *otherUser;
    struct link_Node *channel;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;

    // Sending to another user
    params[0] = cmd->params[0];
    size = 2;
    if(cmd->params[0][0] != '#'){
        otherUser = chat_getUserByName(cmd->params[0]);
        if(otherUser == NULL){
			params[1] = ":Nick not found!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHNICK, params, size);
            return -1;
        }
    } else { // To a channel TODO - Replace this with checkChannelPerms
        channel = chat_getChannelByName(cmd->params[0]);
        if(channel == NULL){
			params[1] = ":Channel not found!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
            return -1;
        } else if (chat_isInChannel(channel, user) == NULL){
			params[1] = ":You do not have permission to send mesesages to this channel!";
            chat_createMessage(reply, user, thisServer, ERR_CANNOTSENDTOCHAN, params, size);
           return -1; 
        }
    }

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);

    params[0] = cmd->params[0];
    params[1] = cmd->params[1];

    chat_createMessage(reply, otherUser, nickname, "PRIVMSG", params, size);
    if(channel == NULL){
        return 1;
    }

	reply->user = user;
    chat_sendChannelMessage(reply, channel);
    return 2;
}

// Join a channel
// TODO - key for access
// TODO - add error checking
const char *join_usage = ":Usage: <channel>";
int cmd_join(struct chat_Message *cmd, struct chat_Message *reply){
    struct chat_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
	int newChannel = -1;
    
    if(cmd->params[0][0] != '#'){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
        return -1;
    }

    struct link_Node *channel = chat_getChannelByName(cmd->params[0]);
    if(channel == NULL){
        channel = chat_createChannel(cmd->params[0], NULL);
		newChannel = 1;

        if(channel == NULL){
           return -2; // Add better error later
        }

        char msg[100] = "Created new channel: ";
        strncat(msg, cmd->params[0], ARRAY_SIZE(msg) - strlen(msg) - 1);
        log_logMessage(msg, INFO);
    }

    struct chat_ChannelUser *chanUser = chat_addToChannel(channel, user); // Check for error
	if(newChannel == 1){
		chanUser->permLevel = 2; // First user is the operator
	}

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);
    params[0] = cmd->params[0];

    chat_createMessage(reply, user, nickname, "JOIN", params, 1);
    chat_sendChannelMessage(reply, channel);

    // Generate a NAMES command reply to the user for this channel
    struct com_QueueJob *job = malloc(sizeof(struct com_QueueJob));
    if(job == NULL){
            log_logError("Error creating job", DEBUG);
            return 2;
    }
    job->type = 1;
    struct chat_Message *jobMsg = malloc(sizeof(struct chat_Message));
    if(jobMsg == NULL){
            log_logError("Error creating command", DEBUG);
            return 2;
    }
    chat_createMessage(jobMsg, user, thisServer, "NAMES", params, 1);
    job->msg = jobMsg;
    job->user = user;
    chat_insertQueue(job);

    return 2;
}

// Returns list of names
// TODO - Hidden/private channels and multiple channels
int cmd_names(struct chat_Message *cmd, struct chat_Message *reply){
    struct chat_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
    
    struct link_Node *channel = chat_getChannelByName(cmd->params[0]);
    if(channel == NULL){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, 1);
        return -1;
    }

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);
    char names[ARRAY_SIZE(cmd->params[0])];
    chat_getUsersInChannel(channel, names, ARRAY_SIZE(names));
    params[0] = nickname;
    params[1] = "=";
    params[2] = cmd->params[0];
    params[3] = names;

    chat_createMessage(reply, user, thisServer, RPL_NAMREPLY, params, 4);
	chat_sendMessage(reply);

	rpl_endofnames(reply, channel->data, user);
    return 1;
}

// Leave a channel
// TODO - add error checking
int cmd_part(struct chat_Message *cmd, struct chat_Message *reply){
    struct chat_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;

    struct link_Node *channel = chat_getChannelByName(cmd->params[0]);
    if(cmd->params[0][0] != '#' || channel == NULL){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
        return -1;
    }

    if(chat_removeUserFromChannel(channel, user) < 0) { // Not in the channel
		err_notonchannel(reply, channel->data, user);
		return 1;
	}

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);
    params[0] = cmd->params[0];
    size = 1;

    chat_createMessage(reply, user, nickname, "PART", params, size);
    chat_sendChannelMessage(reply, channel);

    return 1;
}

// Force a user to leave a channel
// TODO - add error checking
int cmd_kick(struct chat_Message *cmd, struct chat_Message *reply){
    struct chat_UserData *user = cmd->user, *otherUser;
    char *params[ARRAY_SIZE(cmd->params)];
	int size = 2;

    struct link_Node *channel = cmd_checkChannelPerms(reply, cmd->params[0], user, 2);
	if(!channel){
		return -1;
	}

    otherUser = chat_getUserByName(cmd->params[1]);
    if(chat_removeUserFromChannel(channel, otherUser) < 0) { // Not in the channel
		err_usernotinchannel(reply, channel->data, user, cmd->params[1]);
		return -1;
	}

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	if(cmd->params[2][0] != '\0'){
		params[2] = cmd->params[2];
		size = 3;
	}

    chat_createMessage(reply, otherUser, nickname, "KICK", params, size);
    chat_sendChannelMessage(reply, channel);

    return 1;
}

// Edit modes for channels and users
int cmd_mode(struct chat_Message *cmd, struct chat_Message *reply){
	struct chat_UserData *user = cmd->user;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	int isChan = -1;
    if(cmd->params[0][0] == '#'){
		isChan = 1;
	}

	// Make sure all modes are valid
	char op = cmd->params[1][0];
	int hasOp = op == '-' || op == '+' ? 1 : 0; // If there is an operation dont check it
	if(hasOp == 0){
		op = '+'; // If no op given, default to +
	}

	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		if(chat_isValidMode(cmd->params[1][i], isChan) == -1){
			char rpl[20];
			snprintf(rpl, ARRAY_SIZE(rpl), ":No such mode: %c", cmd->params[1][i]);

			params[1] = rpl;
			chat_createMessage(reply, user, thisServer, ERR_UNKNOWNMODE, params, 2);
			return -1;
		}

		// May not set themselves as OP or registered
		if(isChan == -1 && op == '+' && (cmd->params[1][i] == 'o' || cmd->params[1][i] == 'r')){
			return 2; // Say nothing
		}
	}

	//Check whether mode is for channel or user
    if(cmd->params[0][0] != '#'){
		return cmd_modeUser(cmd, reply, op, hasOp);
    }

	return cmd_modeChan(cmd, reply, op, hasOp);
}

// Used by cmd_mode specifically for the user
int cmd_modeUser(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct chat_UserData *user = cmd->user, *otherUser = NULL;
    char *params[ARRAY_SIZE(cmd->params)];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];

	otherUser = chat_getUserByName(cmd->params[0]);
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
		chat_changeUserMode(user, op, cmd->params[1][i]);		
	}

	chat_createMessage(reply, user, cmd->params[0], "MODE", params, 2);
	return 1;
}

// Used by cmd_mode specifically for a channel
int cmd_modeChan(struct chat_Message *cmd, struct chat_Message *reply, char op, int hasOp){
    struct chat_UserData *user = cmd->user;
	struct link_Node *channel = NULL;
    char *params[ARRAY_SIZE(cmd->params)];
	char nickname[NICKNAME_LENGTH];

	// Default values
    params[0] = cmd->params[0];
	params[1] = cmd->params[1];
	params[2] = cmd->params[2];

	channel = cmd_checkChannelPerms(cmd, cmd->params[0], user, 2);
	if(!channel){
		return -1;
	}

	// Set all the modes
	int ret = 1;
	for(int i = hasOp; i < (int) strlen(cmd->params[1]); i++){
		ret = chat_executeChanMode(op, cmd->params[1][i], channel, cmd->params[2]);		
		if(ret == -1){
			err_usernotinchannel(reply, channel->data, user, cmd->params[2]);
			return -1;
		}
	}

	chat_getNickname(nickname, user);
	chat_createMessage(reply, user, nickname, "MODE", params, 3);
	return 1;
}

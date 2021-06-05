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
    cmd_addCommand("NICK", 1, 0, &cmd_nick);
    cmd_addCommand("PRIVMSG", 2, 1, &cmd_privmsg);
    cmd_addCommand("JOIN", 1, 1, &cmd_join);
    cmd_addCommand("NAMES", 1, 1, &cmd_names);
    cmd_addCommand("PART", 1, 1, &cmd_part);

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

			if(chat_userIsRegistered(cmd->user) == -1 && command->permLevel >= 1){
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

// Generate a ERR_NOTONCHANNEL reply
void rpl_notonchannel(struct chat_Message *msg, struct chat_Channel *chan, struct chat_UserData *user){
		char *params[] = {chan->name, ":You are not in this channel!"};
		pthread_mutex_lock(&chan->channelMutex); // To make sure that chan->name is protected
        chat_createMessage(msg, user, thisServer, ERR_NOTONCHANNEL, params, 2);
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
		int isReg = chat_userIsRegistered(user);

		// Set the name in the user's buffer
        pthread_mutex_lock(&user->userMutex);
        strncpy(user->nickname, cmd->params[0], NICKNAME_LENGTH);
        pthread_mutex_unlock(&user->userMutex);

        params[0] = cmd->params[0];
		// User is already registered
		if(isReg == 1){
			chat_createMessage(reply, user, oldName, "NICK", params, 1);
			chat_sendServerMessage(reply);
			return 1;
		}

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
    } else { // To a channel
        channel = chat_getChannelByName(cmd->params[0]);
        if(channel == NULL){
			params[1] = ":Channel not found!";
            chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
            return -1;
        } else if (chat_isInChannel(channel, user) < 0){
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
    int size = 1;
    
    if(cmd->params[0][0] != '#'){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
        return -1;
    }

    struct link_Node *channel = chat_getChannelByName(cmd->params[0]);
    if(channel == NULL){
        channel = chat_createChannel(cmd->params[0], NULL);

        if(channel == NULL){
           return -2; // Add better error later
        }

        char msg[100] = "Created new channel: ";
        strncat(msg, cmd->params[0], ARRAY_SIZE(msg) - strlen(msg) - 1);
        log_logMessage(msg, INFO);
    }

    chat_addToChannel(channel, user); // Check for error

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNickname(nickname, user);
    params[0] = cmd->params[0];
    size = 1;

    chat_createMessage(reply, user, nickname, "JOIN", params, size);
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
    int size = 1;
    
    if(cmd->paramCount != 1){
        params[0] = ":Usage: NAMES <channel>";
        chat_createMessage(reply, user, thisServer, ERR_NEEDMOREPARAMS, params, size);
        return -1;
    }

    struct link_Node *channel = chat_getChannelByName(cmd->params[0]);
    if(channel == NULL){
        params[0] = (char *) invalidChanName;
        chat_createMessage(reply, user, thisServer, ERR_NOSUCHCHANNEL, params, size);
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
    size = 4;

    chat_createMessage(reply, user, thisServer, RPL_NAMREPLY, params, size);
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
		rpl_notonchannel(reply, channel->data, user);
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

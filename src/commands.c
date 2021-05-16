#include "commands.h"

/* 
   This file will put global constant strings above functions because
   those strings will be used by those functions. It is just for
   organization.
*/

struct cmd_CommandList cmd_commandList;
struct chat_Message cmd_defaultError;
char *thisServer = "roundtable.example.com";

int init_commands() {
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&cmd_commandList.commandMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex", ERROR);
        return -1;
    }

    // Init cmd_defaultError
    char *params[] = {":Unknown command: "};
    chat_createMessage(&cmd_defaultError, NULL, thisServer, ERR_UNKNOWNERROR, params, 1);
    cmd_defaultError.userNode = NULL;


    cmd_addCommand(0, "NICK", &cmd_nick);
    cmd_addCommand(1, "PRIVMSG", &cmd_privmsg);
    cmd_addCommand(2, "JOIN", &cmd_join);

    log_logMessage("Successfully initalized commands", INFO);
    return 1;
}

int cmd_addCommand(int numeric, char *word, int (*func)(struct chat_Message *, struct chat_Message *)) {
    struct cmd_Command *command = malloc(sizeof(struct cmd_Command));
    if(command == NULL){
        log_logError("Failed to allocate cmd_Command!", DEBUG);
        return -1;
    }

    command->numeric = numeric;
    strncpy(command->word, word, ARRAY_SIZE(command->word));	
    command->func = func;

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
            ret = command->func(cmd, &reply);
            break;
        }
    }
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    // Unknown command: -1 is reserved for known command error
    if(ret == -2){
        memcpy(&reply, &cmd_defaultError, sizeof(reply));
        reply.userNode = cmd->userNode;
        strncat(reply.params[0], cmd->command, 15);
    }

    if(ret != 2){ // 2 is a request that message is not sent
        chat_sendMessage(&reply);
    }
    return ret;
}

// Changes a user's nickname
const char *nick_usage = ":Usage: NICK <nickname>";
const char *nick_success = ":Welcome to the server!";
const char *nick_inUse = ":Nickname already in use!";
int cmd_nick(struct chat_Message *cmd, struct chat_Message *reply){
    struct link_Node *node = cmd->userNode;
    struct chat_UserData *user;
    char *params[ARRAY_SIZE(cmd->params)];
    char numeric[NUMERIC_SIZE];
    int size = 0;

    user = (struct chat_UserData *) node->data;
    // No nickname given
    if(cmd->params[0][0] == '\0'){
        params[0] = (char *) nick_usage;
        strncpy(numeric, ERR_NONICKNAMEGIVEN, ARRAY_SIZE(numeric)-1);
        size = 1;
        chat_createMessage(reply, node, thisServer, numeric, params, size);
        return 1;
    }

    struct link_Node *otherUserNode = chat_getUserByName(cmd->params[0]);
    if(otherUserNode == NULL) { // No other user has this name
        pthread_mutex_lock(&user->userMutex);
        strncpy(user->nickname, cmd->params[0], NICKNAME_LENGTH);
        pthread_mutex_unlock(&user->userMutex);

        params[0] = cmd->params[0];
        params[1] = (char *) nick_success;
        strncpy(numeric, RPL_WELCOME, ARRAY_SIZE(numeric)-1);
        size = 2;
    } else { // Nickname in use
        params[0] = cmd->params[0];
        params[1] = (char *) nick_inUse;
        strncpy(numeric, ERR_NICKNAMEINUSE, ARRAY_SIZE(numeric)-1);
        size = 2;
    }

    chat_createMessage(reply, node, thisServer, numeric, params, size);
    return 1;
}

// Send a message to user or channel
// TODO - add multiple receivers -> <receiver>{,<receiver>}
const char *privmsg_usage = ":Usage: <receiver> <message>";
const char *privmsg_userNotFound = ":Nick not found!";
int cmd_privmsg(struct chat_Message *cmd, struct chat_Message *reply){
    struct link_Node *node = cmd->userNode;
    char *params[ARRAY_SIZE(cmd->params)];
    char numeric[NUMERIC_SIZE];
    int size = 0;
    
    if(cmd->paramCount != 2){
        params[0] = (char *) privmsg_usage;
        strncpy(numeric, ERR_NEEDMOREPARAMS, ARRAY_SIZE(numeric)-1);
        size = 1;
        chat_createMessage(reply, node, thisServer, numeric, params, size);
        return -1;
    }

    struct link_Node *otherUserNode = chat_getUserByName(cmd->params[0]);
    if(otherUserNode == NULL){
        params[0] = cmd->params[0];
        params[1] = (char *) privmsg_userNotFound;
        strncpy(numeric, ERR_NOSUCHNICK, ARRAY_SIZE(numeric)-1);
        size = 2;
        chat_createMessage(reply, node, thisServer, numeric, params, size);
        return -1;
    }

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNameByNode(nickname, node);

    params[0] = cmd->params[0];
    params[1] = cmd->params[1];
    strncpy(numeric, "PRIVMSG", ARRAY_SIZE(numeric)-1);
    size = 2;

    chat_createMessage(reply, otherUserNode, nickname, numeric, params, size);
    return 1;
}

// Join a channel
// TODO - key for access
// TODO - add error checking
const char *join_usage = ":Usage: <channel>";
const char *join_invalidChanName = ":Invalid channel name: start with #";
int cmd_join(struct chat_Message *cmd, struct chat_Message *reply){
    struct link_Node *node = cmd->userNode;
    char *params[ARRAY_SIZE(cmd->params)];
    int size = 1;
    
    if(cmd->paramCount != 1){
        params[0] = (char *) join_usage;
        chat_createMessage(reply, node, thisServer, ERR_NEEDMOREPARAMS, params, size);
        return -1;
    }

    if(cmd->params[0][0] != '#'){
        params[0] = (char *) join_invalidChanName;
        chat_createMessage(reply, node, thisServer, ERR_NOSUCHCHANNEL, params, size);
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

    chat_addToChannel(channel, node); 

    // Success
    char nickname[NICKNAME_LENGTH];
    chat_getNameByNode(nickname, node);
    params[0] = cmd->params[0];
    size = 1;

    chat_createMessage(reply, node, nickname, "JOIN", params, size);
    chat_sendChannelMessage(reply, channel);
    return 2;
}

#include "commands.h"

/* 
   This file will put global constant strings above functions because
   those strings will be used by those functions. It is just for
   organization.
*/

struct cmd_CommandList cmd_commandList;

int init_commands() {
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&cmd_commandList.commandMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex", ERROR);
        return -1;
    }

    cmd_addCommand(0, "NICK", &cmd_nick);

    return 1;
}

int cmd_addCommand(int numeric, char *word, int (*func)(struct link_Node *, struct chat_Message *)) {
    struct cmd_Command *command = malloc(sizeof(struct cmd_Command));
    command->numeric = numeric;
    strncpy(command->word, word, ARRAY_SIZE(command->word));	
    command->func = func;

    pthread_mutex_lock(&cmd_commandList.commandMutex);
    link_add(&cmd_commandList.commands, command);
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    return 1;
}

int cmd_runCommand(struct link_Node *userNode, struct chat_Message *cmd){
    struct link_Node *node;
    struct cmd_Command *command;

    // Loop thru the commands looking for the same command
    pthread_mutex_lock(&cmd_commandList.commandMutex);
    for(node = cmd_commandList.commands.head; node != NULL; node = node->next){
        if(node->data == NULL){
            continue;
        }

        command = node->data;
        if(!strncmp(command->word, cmd->command, ARRAY_SIZE(command->word))){
            pthread_mutex_unlock(&cmd_commandList.commandMutex);
            return command->func(userNode, cmd);
        }
    }
    pthread_mutex_unlock(&cmd_commandList.commandMutex);

    // Nothing matched: unsuccessful
    printf("%p:(\n", node);
    return -1;
}

// Changes a user's nickname
const char *nick_usage = ":Usage: NICK <nickname>";
const char *nick_success = ":Welcome to the server!";
const char *nick_inUse = ":Nickname already in use!";
int cmd_nick(struct link_Node *node, struct chat_Message *cmd){
    struct chat_UserData *user;
    struct chat_Message reply;
    char *params[ARRAY_SIZE(cmd->params)];
    char numeric[5];
    int size = 0;

    user = (struct chat_UserData *) node->data;
    // No nickname given
    if(cmd->params[0][0] == '\0'){
        params[0] = (char *) nick_usage;
        strncpy(numeric, ERR_NONICKNAMEGIVEN, ARRAY_SIZE(numeric)-1);
        size = 1;
        chat_createMessage(&reply, ":roundtable.example.com", numeric, params, size);
        chat_sendMessage(node, &reply);
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

    chat_createMessage(&reply, ":roundtable.example.com", numeric, params, size);
    chat_sendMessage(node, &reply);

    return 1;
}

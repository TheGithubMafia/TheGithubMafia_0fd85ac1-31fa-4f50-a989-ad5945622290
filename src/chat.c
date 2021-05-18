#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "numerics.h"
#include "communication.h"
#include "logging.h"
#include "chat.h"
#include "linkedlist.h"
#include "commands.h"

struct chat_ServerLists serverLists = {0};
size_t chat_globalUserID = 0;

struct chat_DataQueue dataQueue;

void chat_setMaxUsers(int max){
	serverLists.max = max;
}

int init_chat(){
    //Check that the config data is correct
    if(fig_Configuration.threadsDATA < 1){
            fig_Configuration.threadsDATA = 1;
            log_logMessage("Must have at least 1 data thread! Using 1 data thread", WARNING);
    }	

    // Allocate threads for processing user input 
    dataQueue.threads = calloc(fig_Configuration.threadsDATA, sizeof(pthread_t)); 
    if (dataQueue.threads == NULL){
        log_logError("Error initalizing dataQueue.threads", ERROR);
        return -1;
    }
    
    // Initalize mutex to prevent locking issues
    int ret = pthread_mutex_init(&dataQueue.queueMutex, NULL);
    if (ret < 0){
        log_logError("Error initalizing pthread_mutex", ERROR);
        return -1;
    }

    return chat_setupDataThreads(&fig_Configuration); 
}

void chat_close(){
    free(dataQueue.threads);
}

int chat_setupDataThreads(struct fig_ConfigData *config){
    char buff[BUFSIZ];
    int numThreads = config->threadsDATA;
    int ret = 0;

    for (int i = 0; i < numThreads; i++){
        ret = pthread_create(&dataQueue.threads[i], NULL, chat_processQueue, &dataQueue);
        if (ret < 0){
            log_logError("Error initalizing thread", ERROR);
            return -1;
        }
    }

    snprintf(buff, ARRAY_SIZE(buff), "Successfully processing data on %d threads", numThreads);
    log_logMessage(buff, INFO);

    return numThreads;
}

int chat_insertQueue(struct com_QueueJob *job){
    pthread_mutex_lock(&dataQueue.queueMutex); 
    link_add(&dataQueue.queue, job);
    pthread_mutex_unlock(&dataQueue.queueMutex); 

    return 1;
}

void *chat_processQueue(void *param){
    struct chat_DataQueue *dataQ = param;
    struct timespec delay = {.tv_nsec = 1000000}; // 1ms

    while(1) { 
        // Make sure to set as null to prevent undefined behavior
        struct com_QueueJob *job = NULL;

        // grab from first item in linked list expecting link_Node of user
        pthread_mutex_lock(&dataQ->queueMutex);
        if(link_isEmpty(&dataQ->queue) < 0){
            job = link_remove(&dataQ->queue, 0);
        }
        pthread_mutex_unlock(&dataQ->queueMutex);

        // Nothing to process
        if(job == NULL){
            // Allow other threads time to access mutex
            nanosleep(&delay, NULL); 
            continue;
        }

        if(job->type == 0){ // Text to cmd
            chat_parseInput(job); 
        } else if (job->type == 1){ // Run the cmd
            cmd_runCommand(job->msg);
            free(job->msg);
        }

        free(job);
    }

    return NULL;
}

// TODO - Support multiple cmds in one read
// TODO - Handle null bytes? also handle MAJOR issues with memcpy (size of copied)
int chat_parseInput(struct com_QueueJob *job){
    struct link_Node *node = job->node;
    struct chat_Message *cmd = malloc(sizeof(struct chat_Message));
    struct com_QueueJob *cmdJob;

    if(cmd == NULL){
            log_logError("Error creating cmd struct", DEBUG);
            return -1;
    }
    memset(cmd, 0, sizeof(struct chat_Message)); // Set all memory to 0

    log_logMessage(job->str, MESSAGE);

    // Find where the message ends (\r or \n); if not supplied just take the very end of the buffer
    int length = 0;
    int currentPos = 0, loc = 0; // Helps to keep track of where string should be copied
    for (int i = 0; i < ARRAY_SIZE(job->str); i++){
        if(job->str[i] == '\n' || job->str[i] == '\r'){
            length = i+1;
            job->str[i] = ' ';
            break;
        }

	// Remove any non-printable characters
	if(iscntrl(job->str[i]) == 1){
		job->str[i] = ' ';
	}
    }

    if(job->str[0] == ':'){
       loc = chat_findNextSpace(0, length, job->str);
       if (loc > -1){
           memcpy(cmd->prefix, &job->str[1], loc - 1); // Copy everything except for the ':'
           currentPos = loc + 1;
       }
    }

    loc = chat_findNextSpace(currentPos, length, job->str); 
    if (loc >= 0){
        memcpy(cmd->command, &job->str[currentPos], loc - currentPos);
        currentPos = loc + 1;
    }

    // Fill in up to 15 params
    while (loc > -1 && cmd->paramCount < 15){
       loc = chat_findNextSpace(currentPos, length, job->str);
       if (loc >= 0){
            if(job->str[currentPos] == ':'){ // Colon means rest of string is together
                memcpy(cmd->params[cmd->paramCount], &job->str[currentPos], length - currentPos);
                cmd->paramCount++;
                break;
            }

            memcpy(cmd->params[cmd->paramCount], &job->str[currentPos], loc - currentPos);
            currentPos = loc +1;
            cmd->paramCount++;
       }
    }

    cmd->userNode = node;

    // Create job to execute command
    cmdJob = malloc(sizeof(struct com_QueueJob));
    if(cmdJob == NULL){
            log_logError("Error creating job", DEBUG);
            free(cmd);
            return -1;
    }
    cmdJob->msg = cmd;
    cmdJob->type = 1; // Run as a command

    return chat_insertQueue(cmdJob);
}

int chat_sendMessage(struct chat_Message *msg) {
    if(msg == NULL){
        return -1;
    }

    char str[BUFSIZ];
    chat_messageToString(msg, str, ARRAY_SIZE(str));
    com_sendStr(msg->userNode, str);

    return 1;
}

int chat_createMessage(struct chat_Message *msg, struct link_Node *user, char *prefix, char *cmd, char **params, int paramCount) {
    msg->userNode = user;

    if(prefix != NULL){ // Automatically insert a ':' infront
        msg->prefix[0] = ':';
        strncpy(&msg->prefix[1], prefix, ARRAY_SIZE(msg->prefix)-1);
    }
    strncpy(msg->command, cmd, ARRAY_SIZE(msg->command));

    for (int i = 0; i < paramCount; i++){
            strncpy(msg->params[i], params[i], ARRAY_SIZE(msg->params[i]));
    }

    msg->paramCount = paramCount;

    return 1;
}

int chat_messageToString(struct chat_Message *msg, char *str, int sizeStr) {
    snprintf(str, sizeStr, "%s %s", msg->prefix, msg->command);
    
    int newLen = sizeStr - strlen(str);
    for (int i = 0; i < msg->paramCount && newLen > 0; i++){
            strncat(str, " ", newLen);
            strncat(str, msg->params[i], newLen - 1);
            newLen -= strlen(str);
    }

    return 1;
}

// Locate the next space character 
int chat_findNextSpace(int starting, int size, char *str){
    for(int i = starting; i < size; i++){
        if(str[i] == ' ' || str[i] == '\n' || str[i] == '\r'){
            return i;
        }
    }

    return -1;
}

// Fill in a struct with a user's node
int chat_getNameByNode(char buff[NICKNAME_LENGTH], struct link_Node *node){
    struct chat_UserData *user;
    user = (struct chat_UserData *) node->data;
    if(node == NULL || user == NULL){
        return -1;
    }

    pthread_mutex_lock(&user->userMutex);
    strncpy(buff, user->nickname, NICKNAME_LENGTH);
    pthread_mutex_unlock(&user->userMutex);

    return 1;
}

//Same as other but uses name to find answer
struct link_Node *chat_getUserByName(char name[NICKNAME_LENGTH]){
    struct link_Node *node;
    struct chat_UserData *user;

    pthread_mutex_lock(&serverLists.usersMutex);

    for(node = serverLists.users.head; node != NULL; node = node->next){
            user = node->data;
            pthread_mutex_lock(&user->userMutex);

            if(!strncmp(user->nickname, name, NICKNAME_LENGTH)){
                    pthread_mutex_unlock(&user->userMutex);
                    pthread_mutex_unlock(&serverLists.usersMutex);
                    return node;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    pthread_mutex_unlock(&serverLists.usersMutex);

    return NULL;
}

//Find the user in the serverLists list using the user id
struct link_Node *chat_getUserBySocket(int sock){
    struct link_Node *node;
    struct chat_UserData *user;

    pthread_mutex_lock(&serverLists.usersMutex);

    for(node = serverLists.users.head; node != NULL; node = node->next){
            user = node->data;
            pthread_mutex_lock(&user->userMutex);

            if(user->socketInfo.socket == sock){
                    pthread_mutex_unlock(&user->userMutex);
                    pthread_mutex_unlock(&serverLists.usersMutex);
                    return node;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    pthread_mutex_unlock(&serverLists.usersMutex);

    return NULL;
}

//Find the user in the serverLists list using the user id
struct link_Node *chat_getUserById(size_t id){
    struct link_Node *node;
    struct chat_UserData *user;

    pthread_mutex_lock(&serverLists.usersMutex);

    for(node = serverLists.users.head; node != NULL; node = node->next){
            user = node->data;
            pthread_mutex_lock(&user->userMutex);

            if(user->id == id){
                    pthread_mutex_unlock(&user->userMutex);
                    pthread_mutex_unlock(&serverLists.usersMutex);
                    return node;
            }

            pthread_mutex_unlock(&user->userMutex);
    }

    pthread_mutex_unlock(&serverLists.usersMutex);

    return NULL;
}

//Create a new user and return the node that it is in
struct link_Node *chat_createUser(struct com_SocketInfo *sockInfo, char name[NICKNAME_LENGTH]){
    struct chat_UserData *user;

    pthread_mutex_lock(&serverLists.usersMutex);
    //Plan to remove this maxUser check because it should be only
    //for connected users
    if(serverLists.users.size >= serverLists.max){
            log_logMessage("Server is full", WARNING);
            pthread_mutex_unlock(&serverLists.usersMutex);	
            return NULL;
    }

    user = malloc(sizeof(struct chat_UserData));
    if(user == NULL){
            log_logError("Error adding user", ERROR);
            pthread_mutex_unlock(&serverLists.usersMutex);	
            return NULL;
    }
    //Set user's data
    memset(user, 0, sizeof(struct chat_UserData));
    //eventually get this id from saved user data
    user->id = chat_globalUserID++;
    strncpy(user->nickname, name, NICKNAME_LENGTH);
    memcpy(&user->socketInfo, sockInfo, sizeof(struct com_SocketInfo));

    struct link_Node *userNode = link_add(&serverLists.users, user);

    pthread_mutex_unlock(&serverLists.usersMutex);	

    return userNode;
}

// Delete a user from all occurences
int chat_deleteUser(char name[NICKNAME_LENGTH]){
    // Channels

    // Groups

    // Main list

    // free() actual user
    return name[0];
}

int chat_removeUserFromChannel(struct link_Node *channelNode, struct link_Node *userNode){
    struct link_Node *node;
    struct chat_Channel *channel = channelNode->data;
    int ret = -1;
    int pos = 0;

    pthread_mutex_lock(&channel->channelMutex);
    for(node = channel->users.head; node != NULL; node = node->next){
        if(node != NULL && node->data != NULL){
            if(userNode == node->data){
                // Match
                link_remove(&channel->users, pos);
                ret = 1;
                break;
            }
        }

        pos++;
    }
    pthread_mutex_unlock(&channel->channelMutex);
    
    return ret;
}

int chat_removeUserFromAllChannels(struct link_Node *userNode){
    struct link_Node *node;
    struct link_Node *channel;
    int ret = -1;

    pthread_mutex_lock(&serverLists.channelsMutex);

    for(node = serverLists.channels.head; node != NULL; node = node->next){
            channel = node->data;
            int num = chat_removeUserFromChannel(channel, userNode);

            ret = ret == -1 ? num : ret;
    }

    pthread_mutex_unlock(&serverLists.channelsMutex);
    
    return ret;
}

struct link_Node *chat_getChannelByName(char *name){
    struct link_Node *node;
    struct chat_Channel *channel;

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
struct link_Node *chat_createChannel(char *name, struct chat_Group *group){
    if(name[0] != '#'){
        return NULL;
    }

    struct chat_Channel *channel;
    channel = malloc(sizeof(struct chat_Channel));
    if(channel == NULL){
        log_logError("Error creating channel", ERROR);
        return NULL;
    }

    // TODO - make sure name is legal
    strncpy(channel->name, name, CHANNEL_NAME_LENGTH);

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

// Check if a user is in a channel
int chat_isInChannel(struct link_Node *channelNode, struct link_Node *userNode){
    struct chat_Channel *channel = channelNode->data;

    pthread_mutex_lock(&channel->channelMutex);
    for(struct link_Node *n = channel->users.head; n != NULL; n = n->next){
        if(n->data == userNode){
            pthread_mutex_unlock(&channel->channelMutex);
            return 1;
        }
    }
    pthread_mutex_unlock(&channel->channelMutex);

    return -1;
}

// Add a user to a channel from their node on the main user list
struct link_Node *chat_addToChannel(struct link_Node *channelNode, struct link_Node *userNode){
    struct chat_Channel *channel = channelNode->data;

    if(chat_isInChannel(channelNode, userNode) < 0){
        pthread_mutex_lock(&channel->channelMutex);
        struct link_Node *ret = link_add(&channel->users, userNode);
        pthread_mutex_unlock(&channel->channelMutex);
        
        return ret;
    }

    return NULL;
}

// Will fill a string with a list of users
int chat_getUsersInChannel(struct link_Node *channelNode, char *buff, int size){
    struct link_Node *node;
    struct chat_Channel *channel = channelNode->data;
    char nickname[NICKNAME_LENGTH];
    int pos = 1;

    buff[0] = ':';
    pthread_mutex_lock(&channel->channelMutex);
    for(node = channel->users.head; node != NULL && pos < size; node = node->next){
        chat_getNameByNode(nickname, node->data);
        strncat(buff, nickname, size - pos - 1);
        pos = strlen(buff);

        // Space inbetween users
        buff[pos] = ' ';
        buff[pos + 1] = '\0';
        pos++;
    }
    pthread_mutex_unlock(&channel->channelMutex);

    return 1;
}

// Send a message to every user in a channel
// user list is a list of nodes to user (LIST->node (from main list)->user)
int chat_sendChannelMessage(struct chat_Message *cmd, struct link_Node *channelNode){
    struct link_Node *node;
    struct chat_Channel *channel = channelNode->data;

    pthread_mutex_lock(&channel->channelMutex);
    for(node = channel->users.head; node != NULL; node = node->next){
        cmd->userNode = node->data;
        chat_sendMessage(cmd);
    }
    pthread_mutex_unlock(&channel->channelMutex);

    return 1;
}


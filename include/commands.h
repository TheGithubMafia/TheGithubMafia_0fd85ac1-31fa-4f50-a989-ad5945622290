#ifndef cmds_h
#define cmds_h

#include "chat.h"
#include "linkedlist.h"
#include "numerics.h"

struct chat_Message;

struct cmd_CommandList {
    struct link_List commands;	
    pthread_mutex_t commandMutex;
};

struct cmd_Command {
    int numeric;
    char word[50];
    int (*func)(struct link_Node *, struct chat_Message *);
};

int init_commands();

// Will use parameters to construct a cmd_Command struct
// and adds it to the cmd_commandList struct
int cmd_addCommand(int numeric, char *word, int (*func)(struct link_Node *, struct chat_Message *));

int cmd_runCommand(struct link_Node *userNode, struct chat_Message *cmd);

int cmd_nick(struct link_Node *node, struct chat_Message *cmd);

#endif

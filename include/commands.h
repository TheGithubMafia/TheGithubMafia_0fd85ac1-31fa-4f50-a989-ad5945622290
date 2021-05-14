#ifndef cmds_h
#define cmds_h

#include "chat.h"
#include "linkedlist.h"
#include "numerics.h"

#define NUMERIC_SIZE 15

struct chat_Message;

struct cmd_CommandList {
    struct link_List commands;	
    pthread_mutex_t commandMutex;
};

struct cmd_Command {
    int numeric;
    char word[50];
    int (*func)(struct chat_Message *, struct chat_Message *);
};

int init_commands();

// Will use parameters to construct a cmd_Command struct
// and adds it to the cmd_commandList struct
int cmd_addCommand(int numeric, char *word, int (*func)(struct chat_Message *, struct chat_Message *));

int cmd_runCommand(struct chat_Message *cmd);

int cmd_nick(struct chat_Message *cmd, struct chat_Message *reply);

int cmd_privmsg(struct chat_Message *cmd, struct chat_Message *reply);

#endif

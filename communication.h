#include "config.h"
#ifndef communication_h
#define communication_h

#define ARRAY_SIZE(arr) (int)(sizeof(arr)/sizeof((arr)[0]))

//start server socket based on configuration
int com_startServerSocket(struct fig_ConfigData* data, int forceIPv4);

#endif

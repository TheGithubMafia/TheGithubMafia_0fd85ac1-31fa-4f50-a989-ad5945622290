#ifndef security_h
#define security_h

#include "roundtable.h"

// Compares two strings in constant time for security
int sec_constantStrCmp(char *first, char *second, int length);

#endif

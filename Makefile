irc-server:
	gcc -Wall -Werror -Wextra *.c -g -o irc-server.o -lpthread

test-client:
	gcc -Wall -Werror -Wextra testing.c -o test-client.o

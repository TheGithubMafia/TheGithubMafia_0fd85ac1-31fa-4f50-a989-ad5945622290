irc-server:
	gcc -Wall -Werror -Wextra *.c -o irc-server.o

test-client:
	gcc -Wall -Werror -Wextra testing.c -o test-client.o

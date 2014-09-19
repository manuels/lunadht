#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/wait.h>

#include "safe_assert.h"

int run_dbus(int socket, char *dbus_name);
int run_dht(int socket, int port);

int main(int argc, char *argv[]) {
	int port = 7786;
	int sock[2];
	char *dbus_name = "org.manuel.LunaDHT";
	int res;
	int pid;
	int opt;

	while ((opt = getopt(argc, argv, "b:p:")) != -1) {
		switch (opt) {
		case 'b':
			dbus_name = strdup(optarg);
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default: /* '?' */
			printf("Usage: %s [-b dbus_busname] [-p udp_port]\n", argv[0]);
			exit(1);
		}
	}

	res = socketpair(AF_LOCAL, SOCK_STREAM, 0, sock);
	safe_assert(res > -1);

	pid = fork();
	if (pid == 0)
		run_dht(sock[0], port);
	else
		run_dbus(sock[1], dbus_name);

	wait(&res);

	return 0;
}

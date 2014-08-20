#include <sys/types.h>
#include <sys/socket.h>

#include <unistd.h>
#include <assert.h>

int main(int argc, char *argv[]) {
	int port = 17786;
	int sock[2];
	char *dbus_name = "org.manuel.LunaDHT";
	int res;
	int pid;
	int opt;

	while ((opt = getopt(argc, argv, "b:p:")) != -1) {
		switch (opt) {
		case 'b':
			dbus_name = optarg;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		default: /* '?' */
			fprintf(2, "Usage: %s [-p udp_port] [-b dbus_busname]\n",
				argv[0]);
			exit(1);
		}
	}


	res = socketpair(AF_LOCAL, SOCK_STREAM, 0, sock);
	assert(res > -1);

	pid = fork();
	if (pid == 0)
		run_dht(sock[0], port);
	else
		run_dbus(sock[1], dbus_name);

	wait(&res);

	return 0;
}

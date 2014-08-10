#include <sys/types.h>
#include <sys/socket.h>

#include <assert.h>

int main() {
	int port = 7786;
	int sock[2];
	int res;
	int pid;

	res = socketpair(AF_LOCAL, SOCK_STREAM, 0, sock);
	assert(res > -1);

	pid = fork();
	if (pid == 0)
		run_dht(sock[0], port);
	else
		run_dbus(sock[1]);

	wait(&res);

	return 0;
}

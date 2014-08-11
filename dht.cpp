#include <boost/foreach.hpp>
#include <event.h>

#include "libcage/src/cage.hpp"
#include "ipc.h"

static int sock;

static void
on_joined(bool res) {
	int len;
	struct ipc_message msg;

	msg.type = JOINED;
	msg.joined.result = res;

	printf("join\n");

	len = send(sock, &msg, sizeof(msg), 0);
	assert(len == sizeof(msg));
}

class get_callback
{
public:
	void *m_user_data;

	void operator() (bool result, libcage::dht::value_set_ptr vset) {
		libcage::dht::value_set::iterator it;
		struct ipc_message msg;
		int len;
		char *buf;

		msg.type = RESULT;
		msg.result.user_data = this->m_user_data;
		msg.result.length = vset ? vset->size() : 0;

		len = send(sock, &msg, sizeof(msg), 0);
		assert(len == sizeof(msg));

		if (!result)
			return;

		BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
			buf = val.value.get();

			len = send(sock, &val.len, sizeof(val.len), 0);
			assert(len == sizeof(val.len));

			len = send(sock, buf, val.len, 0);
			assert(len == val.len);
		}
	}
};

static void
get(libcage::cage *cage, unsigned int app_id, char *key, void *user_data)
{
	int len;
	unsigned int *buf;

	len = sizeof(app_id) + strlen(key)+1;
	buf = (unsigned int *) malloc(len);
	buf[0] = app_id;
	memcpy(&(buf[1]), key, strlen(key)+1);

	printf("get: %i ", len);
	for (int i = 0; i < len; ++i)
		printf("%02x", ((unsigned char *) buf)[i]);
	printf("\n");

	get_callback on_get_finished;
	on_get_finished.m_user_data = user_data;
	cage->get(buf, len, on_get_finished);
}

static void
put(libcage::cage *cage, unsigned int app_id, char *key,
	char *value, int unsigned ttl)
{
	int len;
	unsigned int *buf;

	len = sizeof(app_id) + strlen(key)+1;
	buf = (unsigned int *) malloc(len);
	buf[0] = app_id;
	memcpy(&buf[1], key, strlen(key)+1);

	printf("put: %i ", len);
	for (int i = 0; i < len; ++i)
		printf("%02x", ((unsigned char *) buf)[i]);
	printf("\n");
	cage->put(buf, len, value, strlen(value)+1, ttl);
	cage->print_state();
}

static void
on_ipc(int fd, short ev_type, void *user_data) {
	int len;
	char *key;
	char *host;
	char *value;
	struct ipc_message msg;
	int flags = MSG_WAITALL;
	libcage::cage *cage = (libcage::cage *) user_data;

	len = recv(fd, &msg, sizeof(msg), flags);
	assert(len == sizeof(msg));

	switch(msg.type) {
	case JOIN:
		// TODO: timer that retries to join every 60 sec if not connected.
		host = (char *) malloc(msg.join.hostlen);
		len = recv(fd, host, msg.join.hostlen, flags);

		cage->join(host, msg.join.port, on_joined);
		break;

	case GET:
		key = (char *) malloc(msg.get.keylen);

		len = recv(fd, key, msg.get.keylen, flags);
		assert(len == msg.get.keylen);
		assert(strlen(key)+1 == msg.get.keylen);

		get(cage, msg.get.app_id, key, msg.get.user_data);
		free(key);
		break;

	case PUT:
		key = (char *) malloc(msg.put.keylen);
		value = (char *) malloc(msg.put.valuelen);

		len = recv(fd, key, msg.put.keylen, flags);
		assert(len == msg.put.keylen);
		assert(strlen(key)+1 == msg.put.keylen);

		len = recv(fd, value, msg.put.valuelen, flags);
		assert(len == msg.put.valuelen);
		assert(strlen(value)+1 == msg.put.valuelen);

		put(cage, msg.put.app_id, key, value, msg.put.ttl);
		free(key);
		free(value);
		break;

	default:
		assert("should not be reached" && (1!=1));
	}
}

extern "C" int
run_dht(int socket, int port) {
	sock = socket;
	event_init();

	/* setup dht */
	libcage::cage *cage = new libcage::cage();
	cage->set_global();
	cage->open(AF_INET6, port);

	/* setup ipc */
	struct event ev;
	event_set(&ev, sock, EV_READ | EV_PERSIST, on_ipc, cage);
	event_add(&ev, NULL);

	event_dispatch();

	return 0;
}

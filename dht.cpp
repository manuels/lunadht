#include <boost/foreach.hpp>
#include <event.h>

#include "libcage/src/cage.hpp"
#include "ipc.h"

extern "C" {
#include "safe_assert.h"
}

#ifndef MAX
#define MAX(a,b) a>b ? a : b
#endif

static int sock;

static void
on_joined(bool res) {
	int len;
	struct ipc_message msg;

	msg.type = JOINED;
	msg.joined.result = res;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len == sizeof(msg));
}

class get_callback
{
public:
	void *m_user_data;
	unsigned int m_app_id;

	void operator() (bool result, libcage::dht::value_set_ptr vset) {
		libcage::dht::value_set::const_iterator it;
		struct ipc_message msg;
		int len;
		char *buf;

		msg.type = RESULT;
		msg.result.user_data = this->m_user_data;
		msg.result.length = vset ? vset->size() : 0;

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert(len == sizeof(msg));

		if (!result)
			return;

		BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
			buf = val.value.get();

			len = send(sock, &val.len, sizeof(val.len), 0);
			safe_assert(len == sizeof(val.len));

			len = send(sock, buf, val.len, 0);
			safe_assert(len == val.len);
		}
	}
};

static void
get(libcage::cage *cage, unsigned int app_id, char *key, int keylen, void *user_data)
{
	int len;
	unsigned int *buf;

	len = sizeof(app_id) + keylen;
	buf = (unsigned int *) malloc(len);
	buf[0] = app_id;
	memcpy(&(buf[1]), key, keylen);

	get_callback on_get_finished;
	on_get_finished.m_user_data = user_data;
	cage->get(buf, len, on_get_finished);
}

static void
put(libcage::cage *cage, unsigned int app_id, char *key, int keylen,
	char *value, int valuelen, int unsigned ttl)
{
	int len;
	unsigned int *buf;

	len = sizeof(app_id) + keylen;
	buf = (unsigned int *) malloc(len);
	buf[0] = app_id;
	memcpy(&buf[1], key, keylen);

	cage->put(buf, len, value, valuelen, ttl);
}

static void
send_node_list(std::list<libcage::cageaddr> const nodes) {
	char host[MAX(INET6_ADDRSTRLEN, INET_ADDRSTRLEN)];
	std::list<libcage::cageaddr>::const_iterator it;
	struct ipc_message msg;
	unsigned short port;
	int len;
	int length;
	const char *res;
	libcage::in6_ptr in6;
	libcage::in_ptr in4;

	msg.type = NODE_LIST;
	msg.node_list.length = nodes.size();

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len == sizeof(msg));

	for (it = nodes.begin(); it != nodes.end(); it++) {
		if (it->domain == PF_INET6) {
			in6 = boost::get<libcage::in6_ptr>(it->saddr);
			res = inet_ntop(it->domain, &(*in6), host, sizeof(host));
			port = ntohs(in6->sin6_port);
		}
		else if (it->domain == PF_INET) {
			in4 = boost::get<libcage::in_ptr>(it->saddr);
			res = inet_ntop(it->domain, &(*in4), host, sizeof(host));
			port = ntohs(in4->sin_port);
		}
		else
			safe_assert(0==1 && "unknown addr domain");
		safe_assert(res != NULL);

		len = send(sock, &port, sizeof(port), 0);
		safe_assert(len == sizeof(port));

		length = strlen(host)+1;
		len = send(sock, &length, sizeof(length), 0);
		safe_assert(len == sizeof(length));

		len = send(sock, host, length, 0);
		safe_assert(len == length);
	}
}

static void
on_ipc(int fd, short ev_type, void *user_data) {
	ssize_t len;
	char *key;
	char *host;
	char *value;
	char *id;
	std::string str;
	struct ipc_message msg;
	int flags = MSG_WAITALL;
	libcage::cage *cage = (libcage::cage *) user_data;
        std::list<libcage::cageaddr> nodes;

	len = recv(fd, &msg, sizeof(msg), flags);
	safe_assert(len == sizeof(msg));

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
		safe_assert(len == msg.get.keylen);

		get(cage, msg.get.app_id, key, msg.get.keylen, msg.get.user_data);
		free(key);
		break;

	case PUT:
		key = (char *) malloc(msg.put.keylen);
		value = (char *) malloc(msg.put.valuelen);

		len = recv(fd, key, msg.put.keylen, flags);
		safe_assert(len == msg.put.keylen);

		len = recv(fd, value, msg.put.valuelen, flags);
		safe_assert(len == msg.put.valuelen);

		put(cage, msg.put.app_id, key, msg.put.keylen, value, msg.put.valuelen, msg.put.ttl);
		free(key);
		free(value);
		break;

	case NODE_LIST:
		nodes = cage->get_table();
		send_node_list(nodes);
		break;

	case GET_NODE_ID:
		id = (char *) cage->get_id_str().c_str();
		msg.type = GET_NODE_ID;
		msg.node_id.length = strlen(id);

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert(len == sizeof(msg));

		len = send(sock, id, msg.node_id.length, 0);
		safe_assert(len == msg.node_id.length);
		break;

	case SET_NODE_ID:
		id = (char *) malloc(msg.node_id.length);

		len = recv(sock, id, msg.node_id.length, 0);
		safe_assert(len == msg.node_id.length);

		str = std::string(id, len);
		cage->set_id_str(str);

		free(id);
	case QUIT:
		event_loopbreak();
		break;

	default:
		safe_assert("should not be reached" && (1!=1));
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

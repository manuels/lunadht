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
dht_on_joined(bool res) {
	int len;
	struct ipc_message msg = {};

	msg.type = JOINED;
	msg.joined.result = res;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len > 0 && (size_t) len == sizeof(msg));
}

class dht_get_callback
{
public:
	void *m_user_data;
	unsigned int m_app_id;

	void operator() (bool result, libcage::dht::value_set_ptr vset) {
		struct ipc_message msg = {};
		ssize_t len;
		char *buf;
		libcage::dht::value_set::const_iterator it;

		msg.type = RESULT;
		msg.result.user_data = this->m_user_data;
		msg.result.length = vset ? vset->size() : 0;

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert(len > 0 && (int) len == sizeof(msg));

		if (msg.result.length == 0)
			return;

		BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
			size_t length;

			buf = val.value.get();

			length = val.len;
			safe_assert(val.len > 0);

			len = send(sock, &length, sizeof(length), 0);
			safe_assert(len > 0 && (int) len == sizeof(length));

			len = send(sock, buf, val.len, 0);
			safe_assert(len > 0 && (int) len == val.len);
		}
	}
};

static void
dht_get(libcage::cage *cage, unsigned int app_id, char *key, int keylen,
	void *user_data)
{
	int len;
	unsigned int *buf;
	dht_get_callback on_get_finished;

	/* construct lookup key */
	len = sizeof(app_id) + keylen;
	buf = (unsigned int *) malloc(len);
	safe_assert(buf != NULL);

	buf[0] = htonl(app_id);
	memcpy(&(buf[1]), key, keylen);

	on_get_finished.m_user_data = user_data;
	cage->get(buf, len, on_get_finished);

	free(buf);
}

static void
dht_put(libcage::cage *cage, unsigned int app_id, char *key, int keylen,
	char *value, int valuelen, int unsigned ttl)
{
	int len;
	unsigned int *buf;

	/* construct lookup key */
	len = sizeof(app_id) + keylen;
	buf = (unsigned int *) malloc(len);
	safe_assert(buf != NULL);
	buf[0] = htonl(app_id);
	memcpy(&(buf[1]), key, keylen);

	cage->put(buf, len, value, valuelen, ttl);
	free(buf);
}

static void
dht_send_node_list(std::list<libcage::cageaddr> const nodes) {
	unsigned int max_host_len = MAX(INET6_ADDRSTRLEN, INET_ADDRSTRLEN);
	char host[max_host_len];
	std::list<libcage::cageaddr>::const_iterator it;
	struct ipc_message msg = {};
	unsigned short port;
	libcage::in6_ptr in6;
	libcage::in_ptr in4;
	const char *res;
	size_t length;
	int len;

	msg.type = NODE_LIST;
	msg.node_list.length = nodes.size();

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len > 0 && (size_t) len == sizeof(msg));

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
		safe_assert(len > 0 && (size_t) len == sizeof(port));

		length = strlen(host)+1;
		len = send(sock, &length, sizeof(length), 0);
		safe_assert(len > 0 && (size_t) len == sizeof(length));

		len = send(sock, host, length, 0);
		safe_assert(len > 0 && (size_t) len == length);
	}
}

static void
dht_on_ipc(int fd, short ev_type, void *user_data)
{
	libcage::cage *cage = (libcage::cage *) user_data;
	std::list<libcage::cageaddr> nodes;
	struct ipc_message msg = {};
	std::string str;
	ssize_t len;
	char *key;
	char *host;
	char *value;
	char *id;

	len = recv(fd, &msg, sizeof(msg), MSG_WAITALL);
	safe_assert((size_t) len == sizeof(msg));

	switch(msg.type) {
	case JOIN:
		// TODO: timer that retries to join every 60 sec if not connected.
		host = (char *) malloc(msg.join.hostlen);
		safe_assert(host != NULL);
		len = recv(fd, host, msg.join.hostlen, MSG_WAITALL);
		safe_assert((size_t) len == msg.join.hostlen);

		cage->join(host, msg.join.port, dht_on_joined);

		free(host);
		break;

	case GET:
		key = (char *) malloc(msg.get.keylen);
		safe_assert(key != NULL);

		len = recv(fd, key, msg.get.keylen, MSG_WAITALL);
		safe_assert((size_t) len == msg.get.keylen);

		dht_get(cage, msg.get.app_id, key, msg.get.keylen, msg.get.user_data);
		free(key);
		break;

	case PUT:
		key = (char *) malloc(msg.put.keylen);
		value = (char *) malloc(msg.put.valuelen);
		safe_assert(key != NULL);
		safe_assert(value != NULL);

		len = recv(fd, key, msg.put.keylen, MSG_WAITALL);
		safe_assert((size_t) len == msg.put.keylen);

		len = recv(fd, value, msg.put.valuelen, MSG_WAITALL);
		safe_assert((size_t) len == msg.put.valuelen);

		dht_put(cage, msg.put.app_id,
			key, msg.put.keylen,
			value, msg.put.valuelen,
			msg.put.ttl);

		free(key);
		free(value);
		break;

	case NODE_LIST:
		nodes = cage->get_table();
		dht_send_node_list(nodes);
		// TODO: free nodes?
		break;

	case GET_NODE_ID:
		id = (char *) cage->get_id_str().c_str();
		msg.type = GET_NODE_ID;
		msg.node_id.length = strlen(id)+1;

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert((size_t) len == sizeof(msg));

		len = send(sock, id, msg.node_id.length, 0);
		safe_assert((size_t) len == msg.node_id.length);
		break;

	case SET_NODE_ID:
		id = (char *) malloc(msg.node_id.length);
		safe_assert(id != NULL);

		len = recv(sock, id, msg.node_id.length, MSG_WAITALL);
		safe_assert((size_t) len == msg.node_id.length);

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
dht_run(int socket, int port) {
	sock = socket;
	event_init();

	/* setup dht */
	libcage::cage *cage = new libcage::cage();
	cage->set_global();
	cage->open(AF_INET6, port);

	/* setup ipc */
	struct event ev = {};
	event_set(&ev, sock, EV_READ | EV_PERSIST, dht_on_ipc, cage);
	event_add(&ev, NULL);

	event_dispatch();

	return 0;
}

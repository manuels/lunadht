#include <boost/foreach.hpp>
#include <event.h>

#include "libcage/src/cage.hpp"

extern "C" {
#include "ipc.h"
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
	safe_assert_io(len, sizeof(msg), size_t);
}

class dht_get_callback
{
public:
	dht_get_callback(void *user_data) : m_user_data(user_data) {}

	void operator() (bool result, libcage::dht::value_set_ptr vset) {
		struct ipc_message msg = {};
		ssize_t len;
		char *buf;
		libcage::dht::value_set::const_iterator it;

		msg.type = RESULT;
		msg.result.user_data = this->m_user_data;
		msg.result.length = (vset && result) ? vset->size() : 0;

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert_io(len, sizeof(msg), size_t);

		if (msg.result.length == 0)
			return;

		BOOST_FOREACH(const libcage::dht::value_t &val, *vset) {
			size_t length;

			buf = val.value.get();

			length = val.len;
			safe_assert(val.len > 0);

			len = send(sock, &length, sizeof(length), 0);
			safe_assert_io(len, sizeof(length), size_t);

			len = send(sock, buf, val.len, 0);
			safe_assert_io(len, val.len, int);
		}
	}

protected:
	void *m_user_data;
};

static size_t
construct_lookup_key(unsigned int app_id, char *key, size_t keylen,
	unsigned int **buf)
{
	size_t len;

	len = sizeof(app_id) + keylen;
	safe_assert(len > sizeof(app_id) && len > keylen);

	*buf = (unsigned int *) safe_malloc(len);

	(*buf)[0] = htonl(app_id);
	memcpy(&((*buf)[1]), key, keylen);

	return len;
}

static void
dht_get(libcage::cage *cage, unsigned int app_id, char *key, size_t keylen,
	void *user_data)
{
	size_t len;
	unsigned int *buf;
	dht_get_callback on_get_finished(user_data);

	len = construct_lookup_key(app_id, key, keylen, &buf);
	cage->get(buf, len, on_get_finished);

	free(buf);
}

static void
dht_put(libcage::cage *cage, unsigned int app_id, char *key, size_t keylen,
	char *value, int valuelen, int unsigned ttl)
{
	int len;
	unsigned int *buf;

	len = construct_lookup_key(app_id, key, keylen, &buf);
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
	safe_assert_io(len, sizeof(msg), size_t);

	for (it = nodes.begin(); it != nodes.end(); it++) {
		if (it->domain == libcage::domain_inet6) {
			in6 = boost::get<libcage::in6_ptr>(it->saddr);
			res = inet_ntop(AF_INET6, &(in6->sin6_addr), host, sizeof(host));
			port = ntohs(in6->sin6_port);
		}
		else if (it->domain == libcage::domain_inet) {
			in4 = boost::get<libcage::in_ptr>(it->saddr);
			res = inet_ntop(AF_INET, &(in4->sin_addr), host, sizeof(host));
			port = ntohs(in4->sin_port);
		}
		else
			safe_assert(0==1 && "unknown addr domain");
		safe_assert(res != NULL);

		len = send(sock, &port, sizeof(port), 0);
		safe_assert_io(len, sizeof(port), size_t);

		length = strlen(host)+1;
		len = send(sock, &length, sizeof(length), 0);
		safe_assert_io(len, sizeof(length), size_t);

		len = send(sock, host, length, 0);
		safe_assert_io(len, length, size_t);
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
	safe_assert_io(len, sizeof(msg), size_t);

	switch(msg.type) {
	default:
		safe_assert("should not be reached" && (1!=1));
		break;

	case JOIN:
		// TODO: timer that retries to join every 60 sec if not connected.
		host = (char *) safe_malloc(msg.join.hostlen);
		len = recv(fd, host, msg.join.hostlen, MSG_WAITALL);
		safe_assert_io(len, msg.join.hostlen, size_t);

		cage->join(host, msg.join.port, dht_on_joined);

		free(host);
		break;

	case GET:
		key = (char *) safe_malloc(msg.get.keylen);

		len = recv(fd, key, msg.get.keylen, MSG_WAITALL);
		safe_assert_io(len, msg.get.keylen, size_t);

		dht_get(cage, msg.get.app_id, key, msg.get.keylen, msg.get.user_data);
		free(key);
		break;

	case PUT:
		key = (char *) safe_malloc(msg.put.keylen);
		value = (char *) safe_malloc(msg.put.valuelen);

		len = recv(fd, key, msg.put.keylen, MSG_WAITALL);
		safe_assert_io(len, msg.put.keylen, size_t);

		len = recv(fd, value, msg.put.valuelen, MSG_WAITALL);
		safe_assert_io(len, msg.put.valuelen, size_t);

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
		break;

	case GET_NODE_ID:
		id = (char *) cage->get_id_str().c_str();
		msg.type = GET_NODE_ID;
		msg.node_id.length = strlen(id)+1;

		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert_io(len, sizeof(msg), size_t);

		len = send(sock, id, msg.node_id.length, 0);
		safe_assert_io(len, msg.node_id.length, size_t);
		break;

	case SET_NODE_ID:
		id = (char *) safe_malloc(msg.node_id.length);

		len = recv(sock, id, msg.node_id.length, MSG_WAITALL);
		safe_assert_io(len, msg.node_id.length, size_t);

		str = std::string(id, len);
		cage->set_id_str(str);

		free(id);
		break;

	case QUIT:
		event_loopbreak();
		break;
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

	close(sock);

	return 0;
}

enum ipc_type {
	JOIN,
	PUT,
	GET,
	NODE_LIST,
	GET_NODE_ID,
	SET_NODE_ID,
	QUIT,

	RESULT,
	JOINED,
};

struct join_message {
	size_t hostlen;
	unsigned short int port;
};

struct put_message {
	unsigned int app_id;
	size_t keylen;
	size_t valuelen;
	unsigned int ttl;
};

struct get_message {
	unsigned int app_id;
	size_t keylen;
	void *user_data;
};

struct result_message {
	size_t length;
	void *user_data;
};

struct joined_message {
	size_t result;
};

struct node_list_message {
	size_t length;
};

struct node_id_message {
	size_t length;
};

struct ipc_message {
	enum ipc_type type;
	union {
		struct join_message join;
		struct put_message put;
		struct get_message get;
		struct result_message result;
		struct joined_message joined;
		struct node_id_message node_id;
		struct node_list_message node_list; // only valid for dht->dbus, not dbus->dht
	};
};

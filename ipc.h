enum ipc_type {
	JOIN,
	PUT,
	GET,

	RESULT,
	JOINED,
};

struct join_message {
	unsigned int hostlen;
	unsigned short int port;
};

struct put_message {
	unsigned int app_id;
	unsigned int keylen;
	unsigned int valuelen;
	unsigned int ttl;
};

struct get_message {
	unsigned int app_id;
	unsigned int keylen;
	void *user_data;
};

struct result_message {
	int length;
	void *user_data;
};

struct joined_message {
	int result;
};

struct ipc_message {
	enum ipc_type type;
	union {
		struct join_message join;
		struct put_message put;
		struct get_message get;
		struct result_message result;
		struct joined_message joined;
	};
};

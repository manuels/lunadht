#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "ipc.h"
#include "safe_assert.h"
#include "network-bindings.h"

static int sock;
static GSettings *settings = NULL;
static GMainLoop *main_loop;

struct node {
	char *host;
	guint16 port;
};

void
save_node_list(struct node *nodes, int len) {
	if (settings == NULL)
		return;

	GVariantBuilder *builder;
	GVariant *res;

	builder = g_variant_builder_new(G_VARIANT_TYPE("a(aq)"));

	while(len > 0) {
		g_variant_builder_open(builder, G_VARIANT_TYPE("(aq)"));
		//g_variant_builder_add_value(builder,
		//	g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, *results, length[i], sizeof(char)));
		g_variant_builder_close(builder);

		nodes++;
		len--;
	}
	res = g_variant_builder_end(builder);

	g_settings_set_value(settings, "nodes", res);
	g_variant_builder_unref(builder);
}

void
save_node_id(char *id, int len) {
	if (settings == NULL)
		return;

	GVariant *val;
	val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, id, len, sizeof(char));

	g_settings_set_value(settings, "id", val);
	g_variant_unref(val);
}

static gboolean
on_ipc(GIOChannel *src, GIOCondition condition, gpointer user_data)
{
	GDBusMethodInvocation *invocation;
	LunaDHT *dht;
	GVariant *res;
	struct ipc_message msg;
	char *buf;
	char **results;
	struct node *node_list;
	int *length;
	char *id;
	int size;
	int flags = MSG_WAITALL;
	int len;
	int i;

	dht = LUNA_DHT(user_data);

	len = recv(sock, &msg, sizeof(msg), flags);
	safe_assert(len == sizeof(msg));

	switch(msg.type) {
	case JOINED:
		g_debug("joined = %i\n", msg.joined.result);
		g_object_set(dht, "joined", msg.joined.result, NULL);
		break;

	case RESULT:
		g_debug("result len=%i!\n", msg.result.length);

		results = malloc((msg.result.length+1)*sizeof(char *));
		length = malloc((msg.result.length)*sizeof(int));

		for (i = 0; i < msg.result.length; ++i) {
			len = recv(sock, &size, sizeof(size), flags);
			safe_assert(len == sizeof(size));

			buf = malloc(size);
			len = recv(sock, buf, size, flags);
			safe_assert(len == size);

			results[i] = buf;
			length[i] = len;
		}
		results[i] = NULL;

		invocation = (GDBusMethodInvocation *) msg.result.user_data;
		res = g_variant_new_bytestring_array((const gchar * const *) results, -1);
		luna_dht_complete_get(dht, invocation, res);

		g_strfreev(results);
		free(length);
		break;

	case GET_NODE_ID:
		id = malloc(msg.node_id.length);

		len = recv(sock, id, msg.node_id.length, 0);
		safe_assert(len == msg.node_id.length);

		save_node_id(id, len);

		free(id);
		break;

	case NODE_LIST:
		g_debug("node_list len=%i!\n", msg.node_list.length);

		node_list = malloc((msg.node_list.length+1)*sizeof(struct node));
		for (i = 0; i < msg.result.length; ++i) {
			len = recv(sock, &size, sizeof(size), flags);
			safe_assert(len == sizeof(size));

			buf = malloc(size);
			len = recv(sock, buf, size, flags);
			safe_assert(len == size);

			len = recv(sock, &(node_list[i].port), sizeof(node_list[i].port), flags);
			safe_assert(len == sizeof(node_list[i].port));

			node_list[i].host = buf;
		}

		save_node_list(node_list, msg.result.length);
		// TODO: free(node_list[:]);
		free(node_list);

		msg.type = QUIT;
		send(sock, &msg, sizeof(msg), 0);

		g_main_quit(main_loop);
		break;

	default:
		safe_assert("should not be reached " && (0==1));
	}

	return TRUE;
}

static gboolean
on_dbus_join(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    const gchar *host,
    const guint16 port)
{
	int len;
	struct ipc_message msg;

	msg.type = JOIN;
	msg.join.hostlen = strlen(host)+1;
	msg.join.port = port;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len == sizeof(msg));

	len = send(sock, host, strlen(host)+1, 0);
	safe_assert(len == strlen(host)+1);

	if (dht && invocation)
		luna_dht_complete_join(dht, invocation);

	return TRUE;
}

static gboolean
on_dbus_get(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    guint app_id,
    GVariant *arg_key)
{
	struct ipc_message msg;
	const char *key;
	gsize keylen;
	int len;

	key = g_variant_get_fixed_array(arg_key, &keylen, sizeof(char));

	msg.type = GET;
	msg.get.app_id = app_id;
	msg.get.keylen = keylen;
	msg.get.user_data = invocation;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len == sizeof(msg));

	len = send(sock, key, msg.get.keylen, 0);
	safe_assert(len == msg.get.keylen);

	return TRUE;
}

static gboolean
on_dbus_put(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    guint app_id,
    GVariant *arg_key,
    GVariant *arg_value,
    guint64 ttl)
{
	struct ipc_message msg;
	const char *key;
	const char *value;
	gsize keylen;
	gsize valuelen;
	int len;

	key = g_variant_get_fixed_array(arg_key, &keylen, sizeof(char));
	value = g_variant_get_fixed_array(arg_value, &valuelen, sizeof(char));

	msg.type = PUT;
	msg.put.app_id = app_id;
	msg.put.keylen = keylen;
	msg.put.valuelen = valuelen;
	msg.put.ttl = ttl;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert(len == sizeof(msg));

	g_debug("dbus put: len=%lu", keylen);
	len = send(sock, key, msg.put.keylen, 0);
	safe_assert(len == msg.put.keylen);

	len = send(sock, value, msg.put.valuelen, 0);
	safe_assert(len == msg.put.valuelen);

	luna_dht_complete_put(dht, invocation);

	return TRUE;
}

static void
on_service_name_acquired(GDBusConnection *dbus_conn,
	                     const gchar *name,
	                     gpointer user_data) {
	LunaDHT *dht;
	char path[] = "/org/manuel/LunaDHT";
	
	g_debug("DBus name aquired.");

	dht = luna_dht_skeleton_new();
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dht),
	        dbus_conn, path, NULL);

	g_signal_connect(dht, "handle_put", G_CALLBACK(on_dbus_put), dbus_conn);
	g_signal_connect(dht, "handle_get", G_CALLBACK(on_dbus_get), dbus_conn);
	g_signal_connect(dht, "handle_join", G_CALLBACK(on_dbus_join), dbus_conn);

	g_object_set(dht, "joined", FALSE, NULL);

	/* setup ipc */
	GIOChannel *ch = g_io_channel_unix_new(sock);
	g_io_add_watch(ch, G_IO_IN, on_ipc, dht);
}

int load_nodes() {
	if (settings == NULL)
		return -1;

	GVariant *nodes;
	char *host = NULL;
	guint16 port;
	int i;

	settings = g_settings_new_with_path("org.manuel.LunaDHT", ".");

	nodes = g_settings_get_value(settings, "nodes");

	for (i = 0; i < g_variant_n_children(nodes); ++i) {
		g_variant_get_child(nodes, i, "(&sq)", host, &port);
		on_dbus_join(NULL, NULL, host, port);
	}

	g_variant_unref(nodes);

	return 0;
}

void
sig_abort() {
	if(settings == NULL)
		return;

	struct ipc_message msg;

	msg.type = QUIT;
	send(sock, &msg, sizeof(msg), 0);

	g_main_loop_quit(main_loop);
}

void
load_node_id(const char *id, unsigned long len) {
	if (settings == NULL)
		return;

	int length;
	GVariant *val;
	struct ipc_message msg;

	val = g_settings_get_value(settings, "id");
	id = g_variant_get_fixed_array(val, &len, sizeof(char));

	if (id == NULL);
		goto out;

	msg.type = SET_NODE_ID;
	msg.node_id.length = len;

	length = send(sock, &msg, sizeof(msg), 0);
	safe_assert(length == sizeof(msg));

	length = send(sock, id, msg.node_id.length, 0);
	safe_assert(length == msg.node_id.length);

out:
	g_variant_unref(val);
}

int run_dbus(int socket, char *dbus_name) {
	g_type_init();
	signal(SIGABRT, sig_abort);

	int flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	        G_BUS_NAME_OWNER_FLAGS_REPLACE;

	sock = socket;
	main_loop = g_main_loop_new(NULL, TRUE);

	/* setup dbus */
	g_debug("Acquiring DBus name '%s'...", dbus_name);
	g_bus_own_name(G_BUS_TYPE_SESSION, dbus_name, flags,
	        NULL, on_service_name_acquired, NULL, NULL, NULL);

	load_nodes();

	g_main_loop_run(main_loop);

	return 0;
}

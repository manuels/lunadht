#include <sys/types.h>
#include <sys/socket.h>

#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

#include <glib.h>
#include <gio/gio.h>

#include "ipc.h"
#include "safe_assert.h"
#include "settings.h"
#include "network-bindings.h"

int sock;
static GMainLoop *main_loop;

static gboolean
dbus_on_ipc(GIOChannel *src, GIOCondition condition, LunaDHT *dht)
{
	GDBusMethodInvocation *invocation;
	GVariant *res;
	struct ipc_message msg = {};
	char *buf;
	GVariant **results;
	struct node *node_list;
	char *id;
	int flags = MSG_WAITALL;
	size_t size;
	ssize_t len;
	size_t i;

	safe_assert(dht); // TODO: make sure to prevent use-after-free

	len = recv(sock, &msg, sizeof(msg), flags);
	safe_assert_io(len, sizeof(msg), size_t);

	switch(msg.type) {
	case JOINED:
		g_debug("joined = %zu\n", msg.joined.result);
		g_object_set(dht, "joined", msg.joined.result, NULL);
		break;

	case RESULT:
		g_debug("result len=%zu!\n", msg.result.length);

		if (msg.result.length == 0)
			results = NULL;
		else {
			results = calloc(msg.result.length, sizeof(GVariant *));
			safe_assert(results != NULL);
		}

		for (i = 0; i < msg.result.length; ++i) {
			len = recv(sock, &size, sizeof(size), flags);
			safe_assert_io(len, sizeof(size), size_t);

			buf = safe_malloc(size);

			len = recv(sock, buf, size, flags);
			safe_assert_io(len, size, size_t);

			results[i] = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE,  
				buf, len, sizeof(char));

			free(buf);
		}

		invocation = (GDBusMethodInvocation *) msg.result.user_data;
		res = g_variant_new_array(G_VARIANT_TYPE_BYTESTRING,
			results, msg.result.length);
		luna_dht_complete_get(dht, invocation, res);

		break;

	case GET_NODE_ID:
		id = safe_malloc(msg.node_id.length);

		len = recv(sock, id, msg.node_id.length, 0);
		safe_assert_io(len, msg.node_id.length, size_t);
		
		settings_save_node_id(id, len);

		free(id);
		break;

	case NODE_LIST:
		g_debug("node_list len=%zu!\n", msg.node_list.length);

		node_list = calloc(msg.node_list.length, sizeof(struct node));
		safe_assert(node_list != NULL);

		for (i = 0; i < msg.result.length; ++i) {
			len = recv(sock, &size, sizeof(size), flags);
			safe_assert_io(len, sizeof(size), size_t);
			
			safe_assert(size > 0);
			buf = safe_malloc(size);

			len = recv(sock, buf, size, flags);
			safe_assert_io(len, size, size_t);

			len = recv(sock, &(node_list[i].port), sizeof(node_list[i].port), flags);
			safe_assert_io(len, sizeof(node_list[i].port), size_t);

			node_list[i].host = buf;
		}

		settings_save_nodes(node_list, msg.result.length);

		for (i = 0; i < msg.result.length; ++i)
			free(node_list[i].host);
		free(node_list);

		msg.type = QUIT;
		len = send(sock, &msg, sizeof(msg), 0);
		safe_assert_io(len, sizeof(msg), size_t);

		g_main_quit(main_loop);
		break;

	default:
		safe_assert("should not be reached " && (0==1));
		break;
	}

	return TRUE;
}

static gboolean
dbus_on_join(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    const gchar *host,
    const guint16 port)
{
	ssize_t len;
	struct ipc_message msg = {};

	msg.type = JOIN;
	msg.join.hostlen = strlen(host)+1;
	msg.join.port = port;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert_io(len, sizeof(msg), size_t);

	len = send(sock, host, msg.join.hostlen, 0);
	safe_assert_io(len, msg.join.hostlen, size_t);

	if (dht && invocation)
		luna_dht_complete_join(dht, invocation);

	return TRUE;
}

static gboolean
dbus_on_get(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    guint app_id,
    GVariant *arg_key)
{
	struct ipc_message msg = {};
	const char *key;
	gsize keylen;
	ssize_t len;

	key = g_variant_get_fixed_array(arg_key, &keylen, sizeof(char));

	msg.type = GET;
	msg.get.app_id = app_id;
	msg.get.keylen = keylen;
	msg.get.user_data = invocation;

	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert_io(len, sizeof(msg), size_t);

	len = send(sock, key, msg.get.keylen, 0);
	safe_assert_io(len, msg.get.keylen, size_t);

	return TRUE;
}

static gboolean
dbus_on_put(LunaDHT *dht,
    GDBusMethodInvocation *invocation,
    guint app_id,
    GVariant *arg_key,
    GVariant *arg_value,
    guint64 ttl)
{
	struct ipc_message msg = {};
	const char *key;
	const char *value;
	gsize keylen;
	gsize valuelen;
	ssize_t len;

	key = g_variant_get_fixed_array(arg_key, &keylen, sizeof(char));
	value = g_variant_get_fixed_array(arg_value, &valuelen, sizeof(char));

	msg.type = PUT;
	msg.put.app_id = app_id;
	msg.put.keylen = (size_t) keylen;
	msg.put.valuelen = (size_t) valuelen;
	msg.put.ttl = (unsigned int) ttl;

	g_debug("dht-dbus put: keylen=%lu vallen=%lu", keylen, valuelen);
	len = send(sock, &msg, sizeof(msg), 0);
	safe_assert_io(len, sizeof(msg), size_t);

	len = send(sock, key, msg.put.keylen, 0);
	safe_assert_io(len, msg.put.keylen, size_t);

	len = send(sock, value, msg.put.valuelen, 0);
	safe_assert_io(len, msg.put.valuelen, size_t);

	luna_dht_complete_put(dht, invocation);

	return TRUE;
}

static void
dbus_on_name_acquired(GDBusConnection *dbus_conn,
	                     const gchar *name,
	                     gpointer user_data)
{
	GError *err = NULL;
	LunaDHT *dht;
	GIOChannel *ch;
	char path[] = "/org/manuel/LunaDHT";
	
	g_debug("DBus name aquired.");

	dht = luna_dht_skeleton_new();
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(dht),
	        dbus_conn, path, &err);
	safe_assert(!err);

	g_signal_connect(dht, "handle_put", G_CALLBACK(dbus_on_put), dbus_conn);
	g_signal_connect(dht, "handle_get", G_CALLBACK(dbus_on_get), dbus_conn);
	g_signal_connect(dht, "handle_join", G_CALLBACK(dbus_on_join), dbus_conn);

	g_object_set(dht, "joined", FALSE, NULL);

	/* setup ipc */
	ch = g_io_channel_unix_new(sock);
	g_io_add_watch(ch, G_IO_IN, (GIOFunc) dbus_on_ipc, dht);
}

void
dbus_on_sig_abort() {
	if (settings == NULL)
		return;

	struct ipc_message msg = {};
	msg.type = QUIT;

	send(sock, &msg, sizeof(msg), 0);

	g_main_loop_quit(main_loop);
}

int dbus_run(int socket, char *dbus_name) {
	g_type_init();
	signal(SIGABRT, dbus_on_sig_abort);

	int flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	        G_BUS_NAME_OWNER_FLAGS_REPLACE;

	sock = socket;
	main_loop = g_main_loop_new(NULL, TRUE);

	/* setup dbus */
	g_debug("Acquiring DBus name '%s'...", dbus_name);
	g_bus_own_name(G_BUS_TYPE_SESSION, dbus_name, flags,
	        NULL, dbus_on_name_acquired, NULL, NULL, NULL);

	settings_load_nodes();

	g_main_loop_run(main_loop);

	close(sock);

	return 0;
}

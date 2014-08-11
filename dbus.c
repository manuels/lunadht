#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "ipc.h"
#include "network-bindings.h"

static int sock;

static void
present_results(OrgManuelLunaDHT *object,
	GDBusMethodInvocation *invocation,
	char **results)
{
	/*
	GVariantBuilder *builder;
	GVariant *value;

	builder = g_variant_builder_new(G_VARIANT_TYPE("as"));

	while(*results) {
		g_variant_builder_add(builder, "s", *results);
		results++;
	}

	value = g_variant_new("as", builder);
	*/
	g_message("invocation1 %x", invocation);
	org_manuel_luna_dht_complete_get(object, invocation, results);
	g_message("invocation2");

	/*
	g_variant_builder_unref(builder);
	g_variant_unref (value);
	*/
}

static gboolean
on_ipc(GIOChannel *src, GIOCondition condition, gpointer user_data)
{
	OrgManuelLunaDHT *skeleton;
	GDBusMethodInvocation *invocation;
	struct ipc_message msg;
	char *buf;
	char **results;
	int len;
	int size;
	int i;
	int flags;

	skeleton = ORG_MANUEL_LUNA_DHT(user_data);

	flags = MSG_WAITALL;
	len = recv(sock, &msg, sizeof(msg), flags);
	g_assert(len == sizeof(msg));

	switch(msg.type) {
	case JOINED:
		g_debug("joined = %i\n", msg.joined.result);
		g_object_set(skeleton, "joined", msg.joined.result, NULL);
		break;

	case RESULT:
		g_debug("result len=%i!\n", msg.result.length);

		results = malloc((msg.result.length+1)*sizeof(char *));
		for (i = 0; i < msg.result.length; ++i) {
			len = recv(sock, &size, sizeof(size), flags);
			g_assert(len == sizeof(size));

			buf = malloc(size);
			len = recv(sock, buf, size, flags);
			g_assert(len == size);

			results[i] = buf;
		}
		results[i] = NULL;

		invocation = (GDBusMethodInvocation *) msg.result.user_data;
		present_results(skeleton, invocation, results);

		/*
		for (i = 0; i < msg.result.length; ++i)
			free(results[i]);

		free(results);
		*/
		break;

	default:
		g_assert_not_reached();
	}

	return TRUE;
}

static gboolean on_dbus_join(OrgManuelLunaDHT *object,
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
	g_assert(len == sizeof(msg));

	len = send(sock, host, strlen(host)+1, 0);
	g_assert(len == strlen(host)+1);

	org_manuel_luna_dht_complete_join(object, invocation);
}

static gboolean on_dbus_get(OrgManuelLunaDHT *object,
    GDBusMethodInvocation *invocation,
    guint app_id,
    const gchar *key)
{
	int len;
	struct ipc_message msg;

	msg.type = GET;
	msg.get.app_id = app_id;
	msg.get.keylen = strlen(key)+1;
	msg.get.user_data = invocation;

	len = send(sock, &msg, sizeof(msg), 0);
	g_assert(len == sizeof(msg));

	len = send(sock, key, msg.get.keylen, 0);
	g_assert(len == msg.get.keylen);

	return TRUE;
}

static gboolean on_dbus_put(OrgManuelLunaDHT *object,
    GDBusMethodInvocation *invocation,
    guint app_id,
    const gchar *key,
    const gchar *value,
    guint64 ttl)
{
	int len;
	struct ipc_message msg;

	msg.type = PUT;
	msg.put.app_id = app_id;
	msg.put.keylen = strlen(key)+1;
	msg.put.valuelen = strlen(value)+1;
	msg.put.ttl = ttl;

	len = send(sock, &msg, sizeof(msg), 0);
	g_assert(len == sizeof(msg));

	g_debug("dbus put: %s", key);
	len = send(sock, key, msg.put.keylen, 0);
	g_assert(len == msg.put.keylen);

	len = send(sock, value, msg.put.valuelen, 0);
	g_assert(len == msg.put.valuelen);

	org_manuel_luna_dht_complete_put(object, invocation);

	return TRUE;
}

static void
on_service_name_acquired(GDBusConnection *dbus_conn,
	                     const gchar *name,
	                     gpointer user_data) {
	g_debug("DBus name aquired.");

	char path[] = "/org/manuel/LunaDHT";

	OrgManuelLunaDHT *skeleton;
	skeleton = org_manuel_luna_dht_skeleton_new();
	g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(skeleton),
	        dbus_conn, path, NULL);

	g_signal_connect(skeleton, "handle_put",
	        G_CALLBACK(on_dbus_put), dbus_conn);
	g_signal_connect(skeleton, "handle_get",
	        G_CALLBACK(on_dbus_get), dbus_conn);
	g_signal_connect(skeleton, "handle_join",
	        G_CALLBACK(on_dbus_join), dbus_conn);

	g_object_set(skeleton, "joined", FALSE, NULL);

	/* setup ipc */
	GIOChannel *ch = g_io_channel_unix_new(sock);
	g_io_add_watch(ch, G_IO_IN, on_ipc, skeleton);
}


int run_dbus(int socket) {
	g_type_init();

	sock = socket;

	GMainLoop *main_loop;
	main_loop = g_main_loop_new(NULL, TRUE);

	/* setup dbus */
	g_debug("Acquiring DBus name...");
	g_bus_own_name(
	        G_BUS_TYPE_SESSION,
	        "org.manuel.LunaDHT",
	        G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT |
	        G_BUS_NAME_OWNER_FLAGS_REPLACE,
	        NULL,
	        on_service_name_acquired,
	        NULL,
	        NULL,
	        NULL);

	g_main_loop_run(main_loop);

	return 0;
}

#include <sys/socket.h>
#include <string.h>
#include <stdlib.h>

#include <glib.h>
#include <gio/gio.h>

#include "ipc.h"
#include "settings.h"
#include "safe_assert.h"

extern int sock;
GSettings *settings = NULL;

void
settings_save_nodes(struct node *nodes, int len) {
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

int
settings_load_nodes() {
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
		//on_dbus_join(NULL, NULL, host, port); TODO
	}

	g_variant_unref(nodes);

	return 0;
}

void
settings_save_node_id(char *id, int len) {
	if (settings == NULL)
		return;

	GVariant *val;
	val = g_variant_new_fixed_array(G_VARIANT_TYPE_BYTE, id, len, sizeof(char));

	g_settings_set_value(settings, "id", val);
	g_variant_unref(val);
}


void
settings_load_node_id(const char *id, unsigned long len) {
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


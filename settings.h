#ifndef __SETTINGS_H__
#define __SETTINGS_H__

struct node {
	char *host;
	guint16 port;
};

extern GSettings *settings;

void
settings_save_nodes(struct node *nodes, size_t len);

int
settings_load_nodes();

void
settings_save_node_id(char *id, size_t len);

void
settings_load_node_id(const char *id, size_t len);

#endif
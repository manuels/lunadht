#ifndef __SETTINGS_H__
#define __SETTINGS_H__

struct node {
	char *host;
	guint16 port;
};

extern GSettings *settings;

void
save_node_list(struct node *nodes, int len);

void
save_node_id(char *id, int len);

void
load_node_id(const char *id, unsigned long len);

int load_nodes();

#endif
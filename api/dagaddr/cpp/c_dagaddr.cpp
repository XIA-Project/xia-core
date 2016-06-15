#include <map>
#include "dagaddr.h"
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include "dagaddr.hpp"

static struct {
	int xid_type;
	const char *xid_str;
} xidmap[] = {
	{ XID_TYPE_AD, "AD:" },
	{ XID_TYPE_HID, "HID:" },
	{ XID_TYPE_SID, "SID:" },
	{ XID_TYPE_CID, "CID:" },
	{ XID_TYPE_NCID, "NCID:" }
};

const char *get_xid_str(int id)
{
	int i;

	for(i = 0; i < sizeof(xidmap)/sizeof(xidmap[0]); i++) {
		if(xidmap[i].xid_type == id)
			return xidmap[i].xid_str;
	}

	return NULL;
}

#define CHR2HEX(__chr) ((((__chr) >= '0') && ((__chr) <= '9')) ? ((__chr ) - '0') : \
			((__chr >= 'a') ? (10 + (__chr - 'a')) : (10 + (__chr - 'A'))))

#define HEX2CHR(__hex) ((((__hex) >= 0) && ((__hex) <= 9)) ? ((__hex) + '0') : ((__hex) + 'a' - 10))


static void str2hex(unsigned char *hexdst, size_t hexdstlen, char *strsrc, size_t strsrclen)
{
	int srci, dsti;

	for(srci = 0, dsti = 0; (srci < strsrclen) && (dsti < hexdstlen); srci += 2, dsti++) {
		hexdst[dsti] = (CHR2HEX(strsrc[srci]) << 4) | CHR2HEX(strsrc[srci + 1]);
	}
}

void hex2str(char *strdst, size_t strdstlen, unsigned char *hexsrc, size_t hexsrclen)
{
	int srci, dsti;

	for(srci = 0, dsti = 0; (srci < hexsrclen) && (dsti < strdstlen); srci++, dsti += 2) {
		strdst[dsti] = HEX2CHR((hexsrc[srci] & 0xF0) >> 4);
		strdst[dsti + 1] = HEX2CHR(hexsrc[srci] & 0xF);
	}

	strdst[dsti] = 0;
}

static node_t str_to_node(char *xidstr)
{
	char *pos = xidstr;
	node_t node;
	int i;

#define STREQ(__str, __constr) (strncasecmp((__str), (__constr), strlen(__constr)) == 0)
	if(STREQ(pos, "CID")) {
		printf("CID\n");
		node.s_xid.s_type = XID_TYPE_CID;
		pos += 3;
	} else if(STREQ(pos, "HID")) {
		printf("HID\n");
		node.s_xid.s_type = XID_TYPE_HID;
		pos += 3;
	} else if(STREQ(pos, "AD")) {
		printf("AD\n");
		node.s_xid.s_type = XID_TYPE_AD;
		pos += 2;
	} else if(STREQ(pos, "SID")) {
		node.s_xid.s_type = XID_TYPE_SID;
		printf("SID\n");
		pos += 3;
	} else if(STREQ(pos, "IP")) {
		node.s_xid.s_type = XID_TYPE_IP;
		printf("IP\n");
		pos += 2;
	} else if(STREQ(pos, "NCID")) {
		node.s_xid.s_type = XID_TYPE_NCID;
		printf("NCID\n");
		pos += 4;
	} else if(STREQ(pos, "SRC")) {
		node.s_xid.s_type = XID_TYPE_DUMMY_SOURCE;
		printf("Source\n");
		pos += 3;
	}

	pos++;

	str2hex(node.s_xid.s_id, XID_SIZE, pos, strlen(pos));

	for(i = 0; i < EDGES_MAX; i++) {
		node.s_edge[i] = EDGE_UNUSED;
	}

	return node;
}

static char *node_to_str(char *xidstr, size_t xidstrlen, node_t *node)
{
	int xid_type;
	const char *xidstrtype = get_xid_str(node->s_xid.s_type);

	if(xidstr == NULL) {
		return NULL;
	}

	snprintf(xidstr, xidstrlen, "%s:", xidstrtype);
	hex2str(xidstr + strlen(xidstr), xidstrlen, node->s_xid.s_id, XID_SIZE);

	return xidstr;
}

void dag_add_nodes(sockaddr_x *addr, int count, ...)
{
	int i = 0;
	va_list ap;
	va_start(ap, count);
	node_t node;

	memset(addr, 0, sizeof(sockaddr_x));
	addr->sx_family = AF_XIA;
	addr->sx_addr.s_count = count;

	while(count--) {
		char *xid = va_arg(ap, char *);
		addr->sx_addr.s_addr[i] = str_to_node(xid);
		i++;
	}

	va_end(ap);
}

void dag_add_node(sockaddr_x *addr, char *xid)
{
	addr->sx_addr.s_addr[addr->sx_addr.s_count] = str_to_node(xid);
	addr->sx_addr.s_count++;
}

static void add_edge(node_t *node, int dest)
{
	int i;

	for(i = 0; i < EDGES_MAX; i++) {
		if(node->s_edge[i] == EDGE_UNUSED) {
			node->s_edge[i] = dest;
			return;
		}
	}
}

void dag_add_edge(sockaddr_x *addr, int src, int dest)
{
	add_edge(&addr->sx_addr.s_addr[src], dest);
}

static int get_path_recursive(sockaddr_x *scratch, int path[], int *pathlen, int node)
{
#define printf(...)
	int i;
	int ret;

	printf("For Node %d\n", node);
	for(i = 0; i < EDGES_MAX; i++) {
		int edge = scratch->sx_addr.s_addr[node].s_edge[i];
		printf("\tEdge %d\n", edge);

		if(edge == EDGE_UNUSED) {
			printf("\tUnused\n", edge);
			continue;
		}

		/* Check if we are done */
		if(edge == scratch->sx_addr.s_count - 1) {
			printf("\tIntent\n", edge);
			path[(*pathlen)++] = edge;
			scratch->sx_addr.s_addr[node].s_edge[i] = EDGE_UNUSED;
			return 0;
		}

		printf("\tRecursive Try\n", edge);
		ret = get_path_recursive(scratch, path, pathlen, edge);
		if(ret < 0) {
			scratch->sx_addr.s_addr[node].s_edge[i] = EDGE_UNUSED;
		} else {
			path[(*pathlen)++] = edge;
			return ret;
		}

	}

	return -1;
#undef printf
}

static int get_path(sockaddr_x *scratch, int path[])
{
	int i, ret, pathlen = 0;
	node_t *last_node;
	
	ret = get_path_recursive(scratch, path, &pathlen, scratch->sx_addr.s_count - 1);

	if(ret < 0)
		return ret;

	return pathlen;
}

int dag_to_url(char *url, size_t urlsize, sockaddr_x *addr)
{
#define printf(...)
	int i, ret;
	char lurl[256];
	int path[addr->sx_addr.s_count];
	sockaddr_x scratch = *addr;

	printf("DAG2URL\n");

	snprintf(url, urlsize, "dag://");
	sprintf(lurl, "dag://");
	for(i = 0; i < addr->sx_addr.s_count; i++) {
		char type[256], id[256], xid[256];

		strcpy(type, get_xid_str(addr->sx_addr.s_addr[i].s_xid.s_type));
		hex2str(id, 256, addr->sx_addr.s_addr[i].s_xid.s_id, XID_SIZE);

		snprintf(xid, 256, "%s%s", type, id);
		sprintf(lurl + strlen(lurl), ".%s", xid);
	}

	memset(path, 0, sizeof(path));
	while((ret = get_path(&scratch, path)) >= 0) {
		char id[256];

		for(i = ret - 1; i >= 0; i--) {
			hex2str(id, 256, addr->sx_addr.s_addr[path[i]].s_xid.s_id, XID_SIZE);
			if(ret == 1) {
				snprintf(url + strlen(url), urlsize, "%s%s",
					 get_xid_str(addr->sx_addr.s_addr[path[i]].s_xid.s_type),
					 id);
			} else if(i == ret - 1) {
				snprintf(url + strlen(url), urlsize, ".fallback-%s%s",
					 get_xid_str(addr->sx_addr.s_addr[path[i]].s_xid.s_type),
					 id);
			} else {
				snprintf(url + strlen(url), urlsize, "-%s%s",
					 get_xid_str(addr->sx_addr.s_addr[path[i]].s_xid.s_type),
					 id);
			}
			printf(" %d", path[i]);
		}
		printf("\n");
		memset(path, 0, sizeof(path));
	}

	printf("url = %s\n", url);

	return 0;
#undef printf
}

struct node {
	char *xid;
	int ref[EDGES_MAX];
	int new_index;
	int count;
	int traversed;
	int ref_count;
};

int url_to_dag(sockaddr_x *addr, char *url, size_t urlsize)
{
#define printf(...)
	char *saveptr, *token;
	Node n_src;
	Graph g, *g_path = new Graph();
	std::map<std::string, Node *> node_map;
	std::map<std::string, Node *>::iterator node_iter;

	token = strtok_r(url, "/.-", &saveptr);

	*g_path = n_src;
	if(strcmp(token, "dag:") != 0)
		return -1;

	while((token = strtok_r(NULL, "/.-", &saveptr)) != NULL) {

		printf("Token = %s\n", token);
		if(strcmp(token, "fallback") == 0) {
			g += *g_path;
			g_path->print_graph();
			delete g_path;

			g_path = new Graph();
			*g_path = n_src;

		} else {
			Node *n;
			node_iter = node_map.find(std::string(token));

			
			if(node_iter != node_map.end()) {
				printf("Iter found\n");
				n = node_iter->second;
			} else {
				n = new Node(token);

				node_map[std::string(token)] = n;
			}

			*g_path *= *n;
			// printf("Added node\n");
			// g_path->print_graph();
		}
	}
	g += *g_path;

	g.fill_sockaddr(addr);
#undef printf
}

void dag_add_path(sockaddr_x *addr, int count, ...)
{
	int current, first, last, i = 0;
	va_list ap;
	va_start(ap, count);
	node_t node;

	last = va_arg(ap, int);

	if(count == 1) {
		add_edge(&addr->sx_addr.s_addr[addr->sx_addr.s_count - 1], last);
		return;
	}

	first = last;
	count--;
	while(count--) {
		current = va_arg(ap, int);
		add_edge(&addr->sx_addr.s_addr[last], current);
		printf("Adding from %d to %d\n", last, current);
		last = current;

	}

	/* Last node is intent node always */
	add_edge(&addr->sx_addr.s_addr[addr->sx_addr.s_count - 1], first);

	va_end(ap);
}

void dag_set_intent(sockaddr_x *addr, int index)
{
	dag_add_path(addr, 1, index);
}

void dag_set_fallback(sockaddr_x *addr, int index)
{
	dag_add_path(addr, 1, index);
}

void print_sockaddr(sockaddr_x *a)
{
	int i, j;
#define print_var(__var) printf("%s = %x\n", #__var, __var);
	print_var(a->sx_addr.s_count);
	for(i = 0; i < a->sx_addr.s_count; i++) {
		node_t *node = &a->sx_addr.s_addr[i];

		print_var(node->s_xid.s_type);
		for(j = 0; j < EDGES_MAX; j++) {
			print_var(node->s_edge[j]);
		}
	}
}

/*
 * *-------------->SID
 *  \            /
 *   \-->AD->HID/
 */
// int main(void)
// {
// 	sockaddr_x addr, addr1;
	
// 	dag_add_nodes(&addr, 3, "AD:2122", "HID:00000", "CID:299292");
// 	dag_add_path(&addr, 3, 0, 1, 2);
// 	dag_add_path(&addr, 2, 0, 2);
// 	dag_set_intent(&addr, 2);

// 	Graph g;

// 	g.from_sockaddr(&addr);
// 	printf("My address: \n");
// 	g.print_graph();
// 	printf("---\n");

// 	return 0;
// }

#ifndef __DAGADDR_C_H__
#define __DAGADDR_C_H__
#include <stdarg.h>
#include <stdio.h>
#include "xia.h"

#ifdef __cplusplus
extern "C" {
#endif

void dag_add_fallback(sockaddr_x *, int, ...);
void dag_add_nodes(sockaddr_x *addr, int count, ...);
void dag_add_node(sockaddr_x *addr, char *xid);
void dag_add_path(sockaddr_x *addr, int count, ...);
void dag_set_intent(sockaddr_x *addr, int index);
void dag_set_fallback(sockaddr_x *addr, int index);
void dag_add_edge(sockaddr_x *addr, int src, int dest);
int dag_to_url(char *url, size_t urlsize, sockaddr_x *addr);
int url_to_dag(sockaddr_x *addr, char *url, size_t urlsize);

#ifdef __cplusplus
}
#endif

#endif

#ifndef __WRAPPER_H__
#define __WRAPPER_H__

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <strings.h>
#include <time.h>

// Socket
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>

// Thread
#include <pthread.h>

// Semaphore
#include <semaphore.h>

#include "config.h"

typedef struct sockaddr SA;

int listen_on_w(int port);
int accept_w(int s, struct sockaddr *addr, socklen_t *addr_len);
int connect_to(const char *host, int port);
ssize_t write_w(int fd, const void *buf, size_t count);

int pthread_create_w(pthread_t *thread, const pthread_attr_t *attr,
            void *(*start_routine)(void*), void *arg);
int pthread_detach_w(pthread_t thread);

void sem_init_w(sem_t *sem, int pshared, unsigned int value);
void sem_wait_w(sem_t *sem);
void sem_post_w(sem_t *sem);

void *malloc_w(size_t size);

#endif

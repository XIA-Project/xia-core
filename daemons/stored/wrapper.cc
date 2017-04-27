/**
 * wrapper
 * =======
 * Wrappers for different party APIs
 */
#include "wrapper.h"

/* Socket */

/*
 * Listen on port, return the socket if succeed
 */
int listen_on(int port) {

    int listenfd, optval = -1;
    struct sockaddr_in server_addr;

    if((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket construct");
        return -1;
    }

    // avoid address in use error
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                (const void *)&optval, sizeof(int)) < 0) {
        perror("setsocketopt");
        return -1;
    }

    size_t server_addr_len = sizeof(server_addr);
    bzero((char *) &server_addr, server_addr_len);
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons((unsigned short)port);

    if(bind(listenfd, (SA *)&server_addr, server_addr_len) < 0) {
        perror("socket bind");
        return -1;
    }

    if(listen(listenfd, LISTEN_BACKLOG) < 0) {
        perror("socket listen");
        return -1;
    }

    return listenfd;

}

/*
 * Wrapper for listen_on
 */
int listen_on_w(int port) {
    int r;
    if((r = listen_on(port)) < 0) {
        exit(EXIT_FAILURE);
    }
    return r;
}

/*
 * Wrapper for accept
 */
int accept_w(int s, struct sockaddr *addr, socklen_t *addr_len) {
    int r;
    if((r = accept(s, addr, addr_len)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    return r;
}

/*
 * Connect to server:port, exit if error found
 */
int connect_to(const char *host, int port) {

    int clientfd;
    struct hostent *server;
    struct sockaddr_in server_addr;

    if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) <0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    if((server = gethostbyname(host)) == NULL) {
        perror("gethostbyname");
        exit(EXIT_FAILURE);
    }

    bzero((char *)&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    bcopy((char *)server->h_addr,
          (char *)&server_addr.sin_addr.s_addr,
          server->h_length);
    server_addr.sin_port = htons(port);

    if(connect(clientfd, (SA *)&server_addr, sizeof(server_addr)) < 0) {
        perror("connect");
        exit(EXIT_FAILURE);
    }

    return clientfd;

}

ssize_t write_w(int fd, const void *buf, size_t count) {
    ssize_t r;
    if((r = write(fd, buf, count)) < 0) {
        perror("write");
        exit(EXIT_FAILURE);
    }
    return r;
}

/* Thread */

/*
 * Wrapper for pthread_create
 */
int pthread_create_w(pthread_t *thread, const pthread_attr_t *attr,
            void *(*start_routine)(void*), void *arg) {
    int r;
    if((r = pthread_create(thread, attr, start_routine, arg)) != 0) {
        perror("pthread create");
        exit(EXIT_FAILURE);
    }
    return r;
}

/*
 * Wrapper for pthread_detach
 */
int pthread_detach_w(pthread_t thread) {
    int r;
    if((r = pthread_detach(thread)) != 0) {
        perror("pthread detach");
        exit(EXIT_FAILURE);
    }
    return r;
}

/* Semaphore */

void sem_init_w(sem_t *sem, int pshared, unsigned int value) {
    if(sem_init(sem, pshared, value) < 0) {
        perror("sem_init");
        exit(EXIT_FAILURE);
    }
}

void sem_wait_w(sem_t *sem) {
    if(sem_wait(sem) < 0) {
        perror("sem_wait");
    }
}

void sem_post_w(sem_t *sem) {
    if(sem_post(sem) < 0) {
        perror("sem_post");
    }
}

/* Other */

/*
 * Wrapper for malloc
 */
void *malloc_w(size_t size) {
    void *p;
    if((p = malloc(size)) == NULL) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    return p;
}

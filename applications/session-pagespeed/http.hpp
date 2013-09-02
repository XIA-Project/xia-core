#ifndef HTTP_H
#define HTTP_H

#include <stdlib.h>

using namespace std;

#define DEBUG

#ifdef DEBUG
#define LOG(s) fprintf(stderr, "%s:%d: INFO  %s\n", __FILE__, __LINE__, s)
#define LOGF(fmt, ...) fprintf(stderr, "%s:%d: INFO  " fmt"\n", __FILE__, __LINE__, __VA_ARGS__) 
#else
#define LOG(s)
#define LOGF(fmt, ...)
#endif
#define ERROR(s) fprintf(stderr, "\033[0;31m%s:%d: ERROR  %s\n\033[0m", __FILE__, __LINE__, s)
#define ERRORF(fmt, ...) fprintf(stderr, "\033[0;31m%s:%d: ERROR  " fmt"\n\033[0m", __FILE__, __LINE__, __VA_ARGS__) 

int contentLengthFromHTTP(string &httpHeader);
string hostNameFromHTTP(string &httpHeader);
string get_mime_type(const char *name);
int send_header(int ctx, int status, const char *title, const char *extra, const char *mime, 
                  int length, time_t date);
int send_error(int ctx, int status, const char *title, const char *extra, const char *text);
int send_file(int ctx, const char *path, struct stat *statbuf);

#endif /* HTTP_H */

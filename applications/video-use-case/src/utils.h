#ifndef __UTILS_H__
#define __UTILS_H__

#include <time.h>

#define MAX_XID_SIZE 100
#define MAX_PATH_SIZE 1024

#define MB(__mb) (KB(__mb) * 1024)
#define KB(__kb) ((__kb) * 1024)
#define CHUNKSIZE MB(4)

const char MANIFEST_EXTENSION[] = "mpd";
const char XHTTP_INITIAL[] = "xhttp";

void say(const char* fmt, ...);
void warn(const char* fmt, ...);
void die(int ecode, const char* fmt,...);

struct timeval get_current_time(void);
void timeval_add(struct timeval* result, struct timeval* t2, struct timeval* t1);
void timeval_subtract(struct timeval* result, struct timeval* t2, struct timeval* t1);
double calculate_throughput(struct timeval *tv_diff, int bytes);

#endif 
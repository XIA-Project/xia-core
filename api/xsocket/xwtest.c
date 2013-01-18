#include <stdio.h>
#include <sys/socket.h>
#include "Xsocket.h"

#define STREAM_NAME "www_s.stream_echo.aaa.xia"

int setup(int type, char *name)
{
	char *dag;

	printf("\nConnecting to %s server\n", type == SOCK_STREAM ? "stream" : "datagram");

	int fd = socket(AF_XIA, type, 0);
	if (fd < 0) {
		printf("Unable to connect!\n");
		exit(1);
	}

	if ( type == SOCK_STREAM) {

		// FIXME: need to implement name lookup correctly
		printf("\nLooking up dag for %s\n", name);
	    if (!(dag = XgetDAGbyName(name))) {
			printf("unable to locate: %s\n", dag);
			close(fd);
			exit(1);
		}
		printf("\nDAG = %s\n", dag);

		// FIXME: need to use default API for this
		printf("\nConnecting to server\n");

		if (connect(fd, (struct sockaddr *)dag, strlen(dag)) < 0) {
//		if (Xconnect(fd, dag) < 0) {
			printf("can't connect to %s\n", dag);
			close(fd);
			exit(1);
		}

	} else {
		printf("datagram test not implemented yet\n");
		close(fd);
		exit(1);
	}

	return fd;
}

void teardown(int fd)
{
	printf("\nClosing socket %d\n", fd);
	close(fd);
}

void socket_tests(int fd)
{
	const char *XS_TEST = "XSEND TEST STRING\n";

	char buf[256];
	int s, r, match;

	printf("\nRunning socket tests\n");

	printf("\ngetpeername/getsockname tests\n");
	r = sizeof(buf);
	getsockname(fd, (struct sockaddr *)buf, (socklen_t *)&r);
	printf("getsockname: %s\n", buf);
	r = sizeof(buf);
	getpeername(fd, (struct sockaddr *)buf, (socklen_t *)&r);
	printf("getpeername: %s\n", buf);
	

	printf("\nTesting send/recv\n");
	s = send(fd, XS_TEST, strlen(XS_TEST) + 1, 0);
	r = recv(fd, buf, sizeof(buf), 0);
	match = (strcmp(XS_TEST, buf) == 0);
	printf("\nSent:%d Recv:%d %s\n", s, r, match ? "SUCCESS" : "FAIL");

	// untested functions
	// shutdown
	// sockatmark
	// bind
	// connect
	// setsockopt
	// getsockopt
	// socketpair
	// sendmsg
	// recvmsg
	// sendto
	// recvfrom
	// listen

}

void fd_tests(int fd)
{
	const char *rw_test = "read/write test string";
	char reply[256];
	int s, r;

	printf("\nRunning file descriptor tests\n");

	// FIXME: make this use something like gets eventually
	printf("dprintf/recv test\n");
	s = dprintf(fd, "dprintf test string: fd = %d", fd);
	r = recv(fd, reply, sizeof(reply), 0);
	reply[r] = '\0';
	printf("\nsent: %d recv:%d s='%s'\n", s, r, reply);

	printf("\nwrite/read tests\n");
	s = write(fd, rw_test, strlen(rw_test));
	r = read(fd, reply, sizeof(reply));
	if (r >= 0)
		reply[r] = '\0';
	printf("sent:%d recv:%d, s=%s\n", s, r, reply);
}

void file_tests(int fd)
{
	FILE *f;
	char reply[256];
	int s, r;
	char *p;
	char c;

	printf ("\nRunning file i/o tests\n");

	f = fdopen(fd, "r;+");
	printf("\n fd = %d f = %p\n", fd, f);

	printf("\nfprintf/fgets test\n");
	s = fprintf(f, "fprintf test fd = %d f = %p\n", fd, f);
	p = fgets(reply, sizeof(reply), f);
	printf("\nsent:%d recv:%d s='%s'\n", s, strlen(p) + 1, reply);

	printf("\nfgetc test\n");
	r = 0;
	s = fprintf(f, "fgetc test\n");
	while((c = fgetc(f)) != '\n') {
		putchar(c);
		r++;
	}
	printf("sent %d chars, got %d\n", s, r + 1);


	printf("\ngetc test\n");
	r = 0;
	s = fprintf(f, "getc test\n");
	while((c = getc(f)) != '\n') {
		putchar(c);
		r++;
	}
	printf("sent %d chars, got %d\n", s, r + 1);

	printf("\ngetc_unlocked test\n");
	r = 0;
	s = fprintf(f, "getc_unlocked test\n");
	while((c = getc_unlocked(f)) != '\n') {
		putchar(c);
		r++;
	}
	printf("sent %d chars, got %d\n", s, r + 1);

	printf("\nfgetc_unlocked test\n");
	r = 0;
	s = fprintf(f, "fgetc_unlocked test\n");
	while((c = fgetc_unlocked(f)) != '\n') {
		putchar(c);
		r++;
	}
	printf("sent %d chars, got %d\n", s, r + 1);

	printf("\nputc, fputc, putc_unlocked, fputc_unlocked tests\n");
	putc('A', f);
	c = getc(f);
	printf("putc:%c getc:%c\n", 'A', c);

	fputc('B', f);
	c = fgetc(f);
	printf("fputc:%c fgetc:%c\n", 'B', c);

	putc_unlocked('C', f);
	c = getc_unlocked(f);
	printf("putc_unlocked:%c getc_unlocked:%c\n", 'C', c);

	fputc_unlocked('D', f);
	c = fgetc_unlocked(f);
	printf("fputc_unlocked:%c fgetc_unlocked:%c\n", 'D', c);

	printf("\nfputs/fgets tests\n");
	s = fputs("marco\n", f);
	printf("sent %d chars\n", s);
	p = fgets(reply, sizeof(reply), f);
	printf("s:%d r:%d s:%s\n", s, strlen(p), p);

//	FIXME: no definition found for these _unlocked functions, so not testing for now
//	printf("\nfputs_unlocked/fgets_unlocked tests\n");
//	s = fputs_unlocked("polo\n", f);
//	printf("sent %d chars\n", s);
//	p = fgets_unlocked(reply, sizeof(reply), f);
//	printf("s:%d r:%d s:%s\n", s, strlen(p), p);

	printf("\nfwrite/fread test\n");
	char stuff[3][2] = { {'1', '1'}, {'2', '2'}, {'3', '3'} };
	s = fwrite(stuff, 2, 3, f);
	r = fread(reply, 2, 3, f);
	reply[6] = 0;
	printf("sent:%d recv:%d reply:%s\n", s, r, reply);

	printf("\nfwrite_unlocked/fread_unlocked test\n");
	char stuff1[2][3] = { {'4', '4', '4'}, {'5', '5', '5'} };
	s = fwrite_unlocked(stuff1, 3, 2, f);
	r = fread_unlocked(reply, 3, 2, f);
	reply[6] = 0;
	printf("sent:%d recv:%d reply:%s\n", s, r, reply);

	// getline
	// fcloseall

}

// FIXME: this won't work until we can mark the new fds in a way that the
// xsocket api recognizes them as being xsockets. seems to work otherwise.
void stdio_tests(int fd)
{
	int rc;
	char buf[256];

	int sin = dup(STDIN_FILENO);
	int sout = dup(STDOUT_FILENO);	

	printf("\nstdin = %d, stdout = %d\n", fileno(stdin), fileno(stdout));
	dup2(fd, STDIN_FILENO);
	close(STDOUT_FILENO);
	dup2(fd, STDOUT_FILENO);

	fprintf(stderr, "\nstdin = %d, stdout = %d\n", fileno(stdin), fileno(stdout));

	fprintf(stderr, "TRYING stdio remapping!!!\n");
	printf("testing stdio remapping - scary!\n");
	fprintf(stderr, "string sent\n");
	rc = recv(fd, buf, sizeof(buf), 0);
	buf[rc] = 0;
	fprintf(stderr, "%s\n", buf);

	fprintf(stderr, "putting things back!!!\n");
	// FIXME: this is going to cause the underlying click layer to be shutdown, as the fds
	// mapped onto stdin and stdout get closed in the call
	dup2(sin, STDIN_FILENO);
	dup2(sout, STDOUT_FILENO);

	// dup3
	// getchar
	// putchar
	// putchar_unlocked
	// getchar_unlocked
	// gets
	// puts
}



int main()
{
	int fd;

	printf("\nStarting stream test\n");
	fd = setup(SOCK_STREAM, STREAM_NAME);

	socket_tests(fd);
	fd_tests(fd);
	file_tests(fd);

//	stdio_tests(fd);

	teardown(fd);
	return 0;
}
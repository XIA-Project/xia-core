 /* See LICENSE file for license details. */
#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static char *host = "www_s.irc.aaa.xia";
static char *port = "6667";
static char *password;
static char nick[32];
static char bufin[4096];
static char bufout[4096];
static char channel[256];
static time_t trespond;
static int srv;

#include "util.c"

static void
pout(char *channel, char *fmt, ...) {
	static char timestr[18];
	time_t t;
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);
	t = time(NULL);
	strftime(timestr, sizeof timestr, "%D %R", localtime(&t));
	fprintf(stdout, "%-12s: %s %s\n", channel, timestr, bufout);
}

static void
sout(char *fmt, ...) {
	va_list ap;
	int w, l;

	char buf[4096];
	va_start(ap, fmt);
	vsnprintf(bufout, sizeof bufout, fmt, ap);
	va_end(ap);

	w = snprintf(buf, sizeof buf, "%s\r\n", bufout);

#if 0
	fprintf(stdout, "Sending: %s (%d)\n", bufout, w);
	fflush(stdout);
#endif

	if ((l = Xsend(srv, buf, w, 0)) < 0) {
		eprint("unable to write:");
	}

#if 0
	fprintf(stdout, "Sent: (%d)\n", l);
	fflush(stdout);
#endif
}

static void
privmsg(char *channel, char *msg) {
	if(channel[0] == '\0') {
		pout("", "No channel to send to");
		return;
	}
	pout(channel, "<%s> %s", nick, msg);
	sout("PRIVMSG %s :%s", channel, msg);
}

static void
parsein(char *s) {
	char c, *p;

	if(s[0] == '\0')
		return;
	skip(s, '\n');
	if(s[0] != ':') {
		privmsg(channel, s);
		return;
	}
	c = *++s;
	if(c != '\0' && isspace(s[1])) {
		p = s + 2;
		switch(c) {
		case 'j':
			sout("JOIN %s", p);
			if(channel[0] == '\0')
				strlcpy(channel, p, sizeof channel);
			return;
		case 'l':
			s = eat(p, isspace, 1);
			p = eat(s, isspace, 0);
			if(!*s)
				s = channel;
			if(*p)
				*p++ = '\0';
			if(!*p)
				p = "sic - 250 LOC are too much!";
			sout("PART %s :%s", s, p);
			return;
		case 'm':
			s = eat(p, isspace, 1);
			p = eat(s, isspace, 0);
			if(*p)
				*p++ = '\0';
			privmsg(s, p);
			return;
		case 's':
			strlcpy(channel, p, sizeof channel);
			return;
		}
	}
	sout("%s", s);
}

static void
parsesrv_line(char *cmd) {
	char *usr, *par, *txt;

	usr = host;
	if(!cmd || !*cmd)
		return;
	if(cmd[0] == ':') {
		usr = cmd + 1;
		cmd = skip(usr, ' ');
		if(cmd[0] == '\0')
			return;
		skip(usr, '!');
	}
	skip(cmd, '\r');
	par = skip(cmd, ' ');
	txt = skip(par, ':');
	trim(par);
	if(!strcmp("PONG", cmd))
		return;
	if(!strcmp("PRIVMSG", cmd))
		pout(par, "<%s> %s", usr, txt);
	else if(!strcmp("PING", cmd))
		sout("PONG %s", txt);
	else {
		pout(usr, ">< %s (%s): %s", cmd, par, txt);
		if(!strcmp("NICK", cmd) && !strcmp(usr, nick))
			strlcpy(nick, txt, sizeof nick);
	}
}

static void
parsesrv(char *buf) {
	int len = strlen(buf);
	char line[4096];
	char *end;
	while ((end = strstr(buf, "\r\n")) && len > 0) {
		end+=2;
		memcpy(line, buf, end-buf);
		line[end-buf] = '\0';
		parsesrv_line(line);
		len -= end-buf; buf = end;
	}
}


int
main(int argc, char *argv[]) {
	int i, c;
	struct timeval tv;
	const char *user = getenv("USER");
	fd_set rd;

	strlcpy(nick, user ? user : "unknown", sizeof nick);
	for(i = 1; i < argc; i++) {
		c = argv[i][1];
		if(argv[i][0] != '-' || argv[i][2])
			c = -1;
		switch(c) {
		case 'h':
			if(++i < argc) host = argv[i];
			break;
		case 'p':
			if(++i < argc) port = argv[i];
			break;
		case 'n':
			if(++i < argc) strlcpy(nick, argv[i], sizeof nick);
			break;
		case 'k':
			if(++i < argc) password = argv[i];
			break;
		case 'v':
			eprint("sic-\"VERSION\", © 2005-2012 Kris Maglione, Anselm R. Garbe, Nico Golde\n");
		default:
			eprint("usage: sic [-h service_name] [-p port] [-n nick] [-k keyword] [-v]\n");
		}
	}
	/* init */
	srv = dial(host);
	/* login */
	if(password)
		sout("PASS %s", password);
	sout("NICK %s", nick);
	sout("USER %s localhost %s :%s", nick, host, nick);
	for(;;) { /* main loop */
		FD_ZERO(&rd);
		FD_SET(0, &rd);
		FD_SET(srv, &rd);
		tv.tv_sec = 120;
		tv.tv_usec = 0;
		i = Xselect(srv + 1, &rd, 0, 0, &tv);
		if(i < 0) {
			if(errno == EINTR)
				continue;
			eprint("sic: error on Xselect():");
		}
		else if(i == 0) {
			if(time(NULL) - trespond >= 300)
				eprint("sic shutting down: parse timeout\n");
			sout("PING %s", host);
			continue;
		}
		if(FD_ISSET(srv, &rd)) {
			if((c = Xrecv(srv, bufin, sizeof bufin, 0)) < 0)
				eprint("sic: remote host closed connection\n");
			bufin[c] = '\0';
			parsesrv(bufin);
			trespond = time(NULL);
		}
		if(FD_ISSET(0, &rd)) {
			if(fgets(bufin, sizeof bufin, stdin) == NULL)
				eprint("sic: broken pipe\n");
			parsein(bufin);
		}
	}
	return 0;
}
/* vim: set noet nolist: */

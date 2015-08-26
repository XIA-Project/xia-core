#include <stdio.h>
#include <unistd.h>
#include <getopt.h>
#include "Xsocket.h"
#include <stdio.h>
#include <tidy.h>
#include <buffio.h>
#include <fcntl.h>
#include "dagaddr.hpp"
#include <openssl/sha.h>

#define MAX_XID_SIZE 100

#define XID_ONLY(__str) (strchr(__str, ':') + 1)

char ad[MAX_XID_SIZE], hid[MAX_XID_SIZE], four_id[MAX_XID_SIZE];
char *filename;

static std::string hex_str(unsigned char *data, int len)
{
	char hexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
					 '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
	std::string s(len * 2, ' ');
	for (int i = 0; i < len; ++i) {
		s[2 * i]     = hexmap[(data[i] & 0xF0) >> 4];
		s[2 * i + 1] = hexmap[data[i] & 0x0F];
	}
	return s;
}

static std::string compute_cid(const char *data, size_t len)
{
	unsigned char digest[SHA_DIGEST_LENGTH];

	SHA1((unsigned char *)data, len, digest);

	return hex_str(digest, SHA_DIGEST_LENGTH);
}


/* curl write callback, to fill tidy's input buffer...  */ 
uint write_cb(char *in, uint size, uint nmemb, TidyBuffer *out)
{
	uint r;
	r = size * nmemb;
	tidyBufAppend( out, in, r );
	return(r);
}
 
static void print_indent(int indent)
{
	while(indent--)
		printf(" ");
}

void replace_whitespaces(char *dag_str, int len)
{
	char *cur = dag_str;
	int i;

	for(i = 0; i < len; i++) {
		if(isspace(dag_str[i])) {
			dag_str[i] = '.';
		}
	}
}

static void check_for_cid(const char *filename)
{
	Node n_src = Node();
	Node n_ad(Node::XID_TYPE_AD, XID_ONLY(ad));
	Node n_hid(Node::XID_TYPE_HID, XID_ONLY(hid));
	int ret, fd, read_something = 0;
	std::string data("");

	fd = open(filename, O_RDONLY);
	if(fd < 0)
		return;

	while(1) {
		char buf[512];
		if((ret = read(fd, buf, 512)) <= 0) {
			break;
		}
		read_something = 1;
		std::string temp(buf, ret);
		data += temp;
	}

	if(!read_something)
		return;

	Node n_cid(Node::XID_TYPE_CID, compute_cid(data.c_str(), data.length()));
	Graph g0 = n_src * n_cid;
	Graph g1 = n_src * n_ad * n_hid * n_cid;
	Graph g = g0 + g1;
	char buf[g.dag_string().length()];

	strcpy(buf, g.dag_string().c_str());
	replace_whitespaces(buf, g.dag_string().length());

	printf(";%s", buf);
}


/* Traverse the document tree */ 
void dumpNode(TidyDoc doc, TidyNode tnod, int indent)
{
	TidyNode child;
	for (child = tidyGetChild(tnod); child; child = tidyGetNext(child))
	{
		ctmbstr name = tidyNodeGetName(child);
		if(name) {
			/* if it has a name, then it's an HTML tag ... */ 
			TidyAttr attr;
			print_indent(indent);
			printf("<%s", name);
			/* walk the attribute list */ 
			for (attr = tidyAttrFirst(child); attr; attr = tidyAttrNext(attr)) {
				printf(" ");
				printf(tidyAttrName(attr));

				if(strcasecmp(tidyAttrName(attr), "src") == 0) {
					tidyAttrValue(attr) ? printf("=\"%s", tidyAttrValue(attr)) : printf(" ");
					check_for_cid(tidyAttrValue(attr));
					printf("\"");
				} else {
					tidyAttrValue(attr) ? printf("=\"%s\"", tidyAttrValue(attr)) : printf(" ");					
				}
			}
			printf(">\n");
		} else {
			/* if it doesn't have a name, then it's probably text, cdata, etc... */ 
			TidyBuffer buf;
			tidyBufInit(&buf);
			tidyNodeGetText(doc, child, &buf);
			print_indent(indent);
			printf("%s\n", buf.bp ? (char *)buf.bp : "");
			tidyBufFree(&buf);
		}
		dumpNode(doc, child, indent + 4); /* recursive */
		if(name) {
			print_indent(indent);
			printf("</%s>\n", name);
		}
	}
}
 
 
static int read_file(char *filename, TidyBuffer *docbuf)
{
	int ret, fd;
	char buf[512];

	fd = open(filename, O_RDONLY);
	if(fd < 0) {
		perror("open");
		return fd;
	}

	while(1) {
		if((ret = read(fd, buf, 512)) <= 0) {
			return ret;
		}
		write_cb(buf, ret, 1, docbuf);
	}

	return 0;
}

int parse_file(char *filename)
{
	TidyDoc tdoc;
	TidyBuffer docbuf = {0};
	TidyBuffer tidy_errbuf = {0};
	int err;

	tdoc = tidyCreate();
	tidyOptSetBool(tdoc, TidyForceOutput, yes); /* try harder */ 
	tidyOptSetInt(tdoc, TidyWrapLen, 4096);
	tidySetErrorBuffer(tdoc, &tidy_errbuf);
	tidyBufInit(&docbuf);

	read_file(filename, &docbuf);
 
	err = tidyParseBuffer(tdoc, &docbuf); /* parse the input */ 
	if (err >= 0) {
		err = tidyCleanAndRepair(tdoc); /* fix any problems */ 
		if (err >= 0) {
			err = tidyRunDiagnostics(tdoc); /* load tidy error buffer */ 
			if (err >= 0) {
				dumpNode(tdoc, tidyGetRoot(tdoc), 0); /* walk the tree */ 
//				fprintf(stderr, "%s\n", tidy_errbuf.bp); /* show errors */
			}
		}
	}

	return(0);
}

static void usage(char *argv[])
{
	int i;
	printf("Usage: %s [OPTIONS]\n", argv[0]);
	printf("  -i, --input=FILENAME      Input Filename.\n");

	printf("  -h, --help                Displays this help.\n");
}


int main(int argc, char *argv[])
{
	int c, sock;
	char *filename;

	if((sock = Xsocket(AF_XIA, SOCK_STREAM, 0)) < 0) {
		perror("Xsocket");
		return -1;
	}

	if(XreadLocalHostAddr(sock, ad, MAX_XID_SIZE, hid, MAX_XID_SIZE, four_id, MAX_XID_SIZE) < 0) {
		perror("XreadLocalHostAddr");
		return -1;
	}

	struct option options[] = {
		{"input", required_argument, 0, 0},
		{"help", no_argument, 0, 0},
		{0, 0, 0, 0},
	};

	while(1) {
		int option_index = 0;

		c = getopt_long(argc, argv, "i:h", options, &option_index);
		if(c == -1)
			break;

		switch(c) {
		case 0:
			/* long option passed */
			if(!strcmp(options[option_index].name, "input")) {
				filename = (char *)optarg;
			} else if(!strcmp(options[option_index].name, "help")) {
				usage(argv);
				return 0;
			}
			break;
		case 'i':
			filename = (char *)optarg;
			break;
		case 'h':
		default:
			usage(argv);
			return 0;
		}
	}

	parse_file(filename);

	return 0;
}

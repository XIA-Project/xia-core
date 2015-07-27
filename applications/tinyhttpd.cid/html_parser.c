#include <stdio.h>
#include "httpd.h"
#include <tidy.h>
#include <buffio.h>
#include <fcntl.h>
#include <unistd.h>


/* curl write callback, to fill tidy's input buffer...  */ 
uint write_cb(char *in, uint size, uint nmemb, TidyBuffer *out)
{
	uint r;
	r = size * nmemb;
	tidyBufAppend( out, in, r );
	return(r);
}

void publish_cid(ChunkContext *ctx, const char *attr_val, const char *dag)
{
	int i;
	long size = (long)(strchr(attr_val, ';') - attr_val);
	char filename[128] = "htdocs/";
	ChunkInfo *chunks;
	struct stat stat_buf;

	strncat(filename, attr_val, size);

	printf("filename = %s\n", filename);
	if(stat(filename, &stat_buf) < 0) {
		perror("stat");
		return;
	}
	XputFile(ctx, filename, stat_buf.st_size, &chunks);
}

/* Traverse the document tree */ 
void publishCIDs(ChunkContext *ctx, TidyDoc doc, TidyNode tnod)
{
	TidyNode child;
	for (child = tidyGetChild(tnod); child; child = tidyGetNext(child))
	{
		ctmbstr name = tidyNodeGetName(child);
		if(name) {
			/* if it has a name, then it's an HTML tag ... */ 
			TidyAttr attr;
			/* walk the attribute list */ 
			for (attr = tidyAttrFirst(child); attr; attr = tidyAttrNext(attr)) {
				if(strcasecmp(tidyAttrName(attr), "src") == 0) {
					/* Check if this has a DAG */
					const char *val = tidyAttrValue(attr);
					const char *a = val, *b = NULL, *dag = NULL;

					if(strncasecmp(a, "DAG", 3) == 0) {
						dag = a;
						/* First field is a DAG */
					} else if(((b = strchr(val, ';')) != NULL) && (strncasecmp(b + 1, "DAG", 3) == 0)) {
						/* Second field is a DAG */
						dag = b + 1;
					}
					if(dag) {
						printf("Found a DAG: %s\n", dag);
						publish_cid(ctx, dag == a ? b : a, dag);
					}
				}
			}
		}
		publishCIDs(ctx, doc, child); /* recursive */ 
	}
}
 
void read_file_into_buf(TidyBuffer *docbuf, const char *filename)
{
	int ret, fd;
	char buf[512];

	fd = open(filename, O_RDONLY);

	while(1) {
		ret = read(fd, buf, 512);
		if(ret <= 0)
			break;
		write_cb(buf, ret, 1, docbuf);
	}
}
 
int html_parser_init(ChunkContext *ctx)
{
	TidyDoc tdoc;
	TidyBuffer docbuf = {0};
	TidyBuffer tidy_errbuf = {0};
	int err;
 
	tdoc = tidyCreate();
	tidyOptSetBool(tdoc, TidyForceOutput, yes); /* try harder */ 
	tidyOptSetInt(tdoc, TidyWrapLen, 4096);
	tidySetErrorBuffer( tdoc, &tidy_errbuf );
	tidyBufInit(&docbuf);
 
	read_file_into_buf(&docbuf, "htdocs/index.html");

	err = tidyParseBuffer(tdoc, &docbuf); /* parse the input */ 
	if(err >= 0) {
		err = tidyCleanAndRepair(tdoc); /* fix any problems */ 
		if(err >= 0) {
			err = tidyRunDiagnostics(tdoc); /* load tidy error buffer */ 
			if(err >= 0) {
				publishCIDs(ctx, tdoc, tidyGetRoot(tdoc)); /* walk the tree */ 
//				fprintf(stderr, "%s\n", tidy_errbuf.bp); /* show errors */ 
			}
		}
	}
 
	/* clean-up */ 
	tidyBufFree(&docbuf);
	tidyBufFree(&tidy_errbuf);
	tidyRelease(tdoc);

	return(err);
}    

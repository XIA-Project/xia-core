#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "xcache.h"
#include "xcachePriv.h"

int XputFile(ChunkContext *ctx, const char *fname, unsigned chunkSize, ChunkInfo **chunks)
{
	FILE *fp;
	struct stat fs;
	ChunkInfo *chunkList;
	unsigned numChunks;
	unsigned i;
	int rc;
	int count;
	char *buf;

	if (ctx == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (fname == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;
	else if (chunkSize > XIA_MAXBUF)
		chunkSize = XIA_MAXBUF;

	if (stat(fname, &fs) != 0)
		return -1;

	if (!(fp = fopen(fname, "rb")))
		return -1;

	numChunks = fs.st_size / chunkSize;
	if (fs.st_size % chunkSize)
		numChunks ++;
	//FIXME: this should be numChunks, sizeof(ChunkInfo)
	if (!(chunkList = (ChunkInfo *)calloc(numChunks, chunkSize))) {
		fclose(fp);
		return -1;
	}

	if (!(buf = (char*)malloc(chunkSize))) {
		free(chunkList);
		fclose(fp);
		return -1;
	}

	i = 0;
	while (!feof(fp)) {
		if ((count = fread(buf, sizeof(char), chunkSize, fp)) > 0) {
			rc = XputChunk(ctx, buf, count, &chunkList[i]);
			if(rc < 0)
				break;
			if(rc == xcache_cmd::XCACHE_ERR_EXISTS) {
				continue;
			}
			i++;
		}
	}

	rc = i;

	*chunks = chunkList;
	fclose(fp);
	free(buf);

	return rc;
}

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include "xcache.h"
#include "xcachePriv.h"

/* API */
int XputBuffer(ChunkContext *ctx, const char *data, unsigned length, unsigned chunkSize, ChunkInfo **chunks)
{
	ChunkInfo *chunkList;
	unsigned numChunks;
	unsigned i;
	int rc;
	char *buf;
	unsigned offset;

	if (ctx == NULL) {
		errno = EFAULT;
		return -1;
	}

	if (chunkSize == 0)
		chunkSize =  DEFAULT_CHUNK_SIZE;
	else if (chunkSize > XIA_MAXBUF)
		chunkSize = XIA_MAXBUF;

	numChunks = length / chunkSize;
	if (length % chunkSize)
		numChunks ++;
	//FIXME: this should be numChunks, sizeof(ChunkInfo)
	if (!(chunkList = (ChunkInfo *)calloc(numChunks, chunkSize))) {
		return -1;
	}

	if (!(buf = (char*)malloc(chunkSize))) {
		free(chunkList);
		return -1;
	}

	i = 0;
	offset = 0;
#ifndef MIN
#define MIN(__x, __y) ((__x) < (__y) ? (__x) : (__y))
#endif
	while (offset < length) {
		int to_copy = MIN(length - offset, chunkSize);
		memcpy(buf, data + offset, to_copy);
		rc = XputChunk(ctx, buf, to_copy, &chunkList[i]);
		if(rc < 0)
			break;
		if(rc == xcache_cmd::XCACHE_ERR_EXISTS) {
			continue;
		}
		offset += to_copy;
		i++;
	}

	rc = i;

	*chunks = chunkList;
	free(buf);

	return rc;
}

/* API */
int XputChunk(ChunkContext *ctx, const char *data, unsigned length, ChunkInfo *info)
{
	xcache_cmd cmd;

	cmd.set_cmd(xcache_cmd::XCACHE_STORE);
	cmd.set_context_id(ctx->contextID);
	cmd.set_data(data, length);

	if(send_command(&cmd) < 0) {
		printf("%s: Error in sending command to xcache\n", __func__);
		/* Error in Sending chunk */
		return -1;
	}

	if(get_response_blocking(&cmd) < 0) {
		printf("Did not get a valid response from xcache\n");
		return -1;
	}

	if(cmd.cmd() == xcache_cmd::XCACHE_ERROR) {
		if(cmd.status() == xcache_cmd::XCACHE_ERR_EXISTS) {
			printf("%s: Error this chunk already exists\n", __func__);
			return xcache_cmd::XCACHE_ERR_EXISTS;
		}
	}

	printf("%s: Got a response from server\n", __func__);
	strcpy(info->cid, cmd.cid().c_str());

	return xcache_cmd::XCACHE_OK;
}

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

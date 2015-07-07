#include <stdio.h>
#include <errno.h>
#include <sys/stat.h>
#include "xcache.h"
#include "xcachePriv.h"

int XcachePutFile(struct xcacheSlice *slice, const char *fname, unsigned chunkSize, struct xcacheChunk **chunks)
{
  FILE *fp;
  struct stat fs;
  struct xcacheChunk *chunkList;
  unsigned numChunks;
  unsigned i;
  int rc;
  int count;
  char *buf;

  if (slice == NULL) {
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

  if (!(fp= fopen(fname, "rb")))
    return -1;

  numChunks = fs.st_size / chunkSize;
  if (fs.st_size % chunkSize)
    numChunks ++;
  //FIXME: this should be numChunks, sizeof(ChunkInfo)
  if (!(chunkList = (struct xcacheChunk *)calloc(numChunks, chunkSize))) {
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
      chunkList[i].buf = buf;
      chunkList[i].len = count;
      
      if ((rc = XcachePutChunk(slice, &chunkList[i])) < 0)
        break;
      i++;
    }
  }

  if (i != numChunks) {
    // FIXME: something happened, what do we want to do in this case?
    rc = -1;
  }
  else
    rc = i;

  *chunks = chunkList;
  fclose(fp);
  free(buf);

  return rc;
}

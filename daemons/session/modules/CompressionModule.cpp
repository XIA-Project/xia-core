/*
** Copyright 2013 Carnegie Mellon University
**
** Licensed under the Apache License, Version 2.0 (the "License");
** you may not use this file except in compliance with the License.
** You may obtain a copy of the License at
**
**    http://www.apache.org/licenses/LICENSE-2.0
**
** Unless required by applicable law or agreed to in writing, software
** distributed under the License is distributed on an "AS IS" BASIS,
** WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
** See the License for the specific language governing permissions and
** limitations under the License.
*/

#include <assert.h>
#include "zlib.h"
#include "CompressionModule.h"

#if defined(MSDOS) || defined(OS2) || defined(WIN32) || defined(__CYGWIN__)
#  include <fcntl.h>
#  include <io.h>
#  define SET_BINARY_MODE(file) setmode(fileno(file), O_BINARY)
#else
#  define SET_BINARY_MODE(file)
#endif

#define CHUNK 16384

void CompressionModule::decide(session::SessionInfo *sinfo, UserLayerInfo &userInfo, 
										 AppLayerInfo &appInfo, 
										 TransportLayerInfo &transportInfo, 
										 NetworkLayerInfo &netInfo, 
										 LinkLayerInfo &linkInfo, 
										 PhysicalLayerInfo &physInfo) {
	
	(void)transportInfo;
	(void)netInfo;
	(void)linkInfo;
	(void)physInfo;

	if (userInfo.getCompress() || appInfo.checkAttribute(kCompression)) {
		DBG("Using compression");
		sinfo->set_use_compression(true);
	} else {
		DBG("Not using compression");
		sinfo->set_use_compression(false);
	}
}

bool CompressionModule::breakpoint(Breakpoint breakpoint, struct breakpoint_context *context, void *rv) {

	switch(breakpoint) {
		case kSendPreSend:
			DBG("kSendPreSend");
			return preSend(context, rv);

		case kRecvPostRecv:
			DBG("kRecvPostRecv");
			return postRecv(context, rv);

		default:
			DBG("Other breakpoint");
			return true;
	}
}

bool CompressionModule::preSend(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	struct send_args *args = (struct send_args*)context->args;
	session::SessionInfo *sinfo = context->sinfo;
	char *buf = args->buf;
	int msglen = *(args->msglen);
	int buflen = args->buflen;

	if (sinfo == NULL) {
		WARN("Session info was null; defaulting to no compression");
		return true;
	}

	if (sinfo->use_compression()) {
	    //*(int*)rv =???;
		unsigned char temp[CHUNK];
		memcpy(temp, buf, msglen);
		int compressed_len = compress(temp, msglen, (unsigned char*)buf, buflen);

		if (compressed_len < 0) {
			ERROR("Error compressing data");
			return true;
		}

		*(args->msglen) = compressed_len; // tell future blocks the new message length
		DBGF("Compressed %d bytes to %d bytes", msglen, compressed_len);
	}
		
	return true;
}

bool CompressionModule::postRecv(struct breakpoint_context *context, void *rv) {
	(void)rv;
	
	struct recv_args *args = (struct recv_args*)context->args;
	session::SessionInfo *sinfo = context->sinfo;
	char *buf = args->buf;
	int msglen = *(args->msglen);
	int buflen = args->buflen;

	if (sinfo == NULL) {
		WARN("Session info was null; defaulting to no compression");
		return true;
	}

	if (sinfo->use_compression()) {
	    //*(int*)rv =???;
		unsigned char temp[CHUNK];
		memcpy(temp, buf, msglen);
		int decompressed_len = decompress(temp, msglen, (unsigned char*)buf, buflen);

		if (decompressed_len < 0) {
			ERROR("Error decompressing data");
			return true;
		}

		*(args->msglen) = decompressed_len; // tell future blocks the new message length
		DBGF("Decompressed %d bytes to %d bytes", msglen, decompressed_len);
	}
		
	return true;
}


// returns -1 on error, num bytes of compressed data otherwise
int CompressionModule::compress(unsigned char *in, size_t in_len, unsigned char *out, size_t out_len) {
	int ret, flush, bytes_compressed = 0;
    unsigned have;
    z_stream strm;
    //unsigned char in[CHUNK];
    //unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    ret = deflateInit(&strm, Z_DEFAULT_COMPRESSION);
    if (ret != Z_OK)
        return -1;

    /* compress until end of in buffer */
    do {
        strm.avail_in = in_len; //fread(in, 1, CHUNK, source);
        flush = Z_FINISH; //feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = out_len; // CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            have = out_len - strm.avail_out; //CHUNK - strm.avail_out;
			bytes_compressed += have;
        } while (strm.avail_out == 0);
        assert(strm.avail_in == 0);     /* all input will be used */

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    assert(ret == Z_STREAM_END);        /* stream will be complete */

    /* clean up and return */
    (void)deflateEnd(&strm);
    return bytes_compressed;
}

// returns -1 on error; num bytes decompressed data otherwise
int CompressionModule::decompress(unsigned char *in, size_t in_len, unsigned char *out, size_t out_len) {
	int ret, bytes_decompressed = 0;
    unsigned have;
    z_stream strm;
    //unsigned char in[CHUNK];
    //unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    ret = inflateInit(&strm);
    if (ret != Z_OK)
        return -1;

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = in_len; //fread(in, 1, CHUNK, source);
        if (strm.avail_in == 0)
            break;
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = out_len; //CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            assert(ret != Z_STREAM_ERROR);  /* state not clobbered */
            switch (ret) {
            	case Z_NEED_DICT:
            	    ret = Z_DATA_ERROR;     /* and fall through */
            	case Z_DATA_ERROR:
            	case Z_MEM_ERROR:
            	    (void)inflateEnd(&strm);
            	    return -1;
            }
            have = out_len - strm.avail_out;   //CHUNK - strm.avail_out;
			bytes_decompressed += have;
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
	if (ret == Z_STREAM_END)
		return bytes_decompressed;
	else
		return -1;
}

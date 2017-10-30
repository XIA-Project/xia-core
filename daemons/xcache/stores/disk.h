#ifndef __DISK_H__
#define __DISK_H__

#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <syslog.h>
#include <errno.h>
#include <map>
#include "../meta.h"

#define READ_SIZE (1024 * 1024)
#ifndef MIN
#define MIN(X,Y) ((X) < (Y) ? (X) : (Y))
#endif
/**
 * DiskStore:
 * @brief Content Store - [On Disk Storage]
 * A simple content store that stores content on disk
 */

class DiskStore:public xcache_content_store {
private:
	std::string content_dir;

public:
	DiskStore()
	{
		content_dir = "/tmp/content/";
		mkdir(content_dir.c_str(), 0777);
	}

	int store(xcache_meta *meta, const std::string &data)
	{
		int ret, fd;
		std::string path;

		path = content_dir + meta->store_id();

		fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
		if (fd < 0) {
			syslog(LOG_ERR, "Unable to open file %s for writing", path.c_str());
			return -1;
		}

		syslog(LOG_INFO, "Opened file %s for writing", path.c_str());

		ret = 0;

		int rc = write(fd, data.c_str(), data.length());
		if ((unsigned)rc != data.length()) {
			syslog(LOG_ERR, "write requested:%lu actual:%d", data.length(), rc);
			ret = -1;
		}

		close(fd);

		return ret;
	}

	std::string get(xcache_meta *meta)
	{
		int ret, fd, bytes;
		std::string path;
		char *buf = (char *)malloc(READ_SIZE);

		if (!buf) {
			return "";
		}

		path = content_dir + meta->store_id();

		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			syslog(LOG_ERR, "Unable to open %s for reading: %s", path.c_str(), strerror(errno));
			free(buf);
			return "";
		}

		std::string data("");
		bytes = 0;

		while ((ret = read(fd, buf, READ_SIZE)) != 0) {
			std::string temp(buf, ret);
			data += temp;
			bytes += ret;
		}

		close(fd);
		free(buf);

		return data;
	}

	std::string get_partial(xcache_meta *meta, off_t off, size_t len)
	{
		unsigned remaining;
		int ret, fd;
		std::string path;
		char *buf = (char *)malloc(READ_SIZE);

		path = content_dir + meta->store_id();

		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			syslog(LOG_ERR, "Unable to open %s for reading: %s", path.c_str(), strerror(errno));
			return "";
		}

		std::string data("");
		remaining = len;

		lseek(fd, off, SEEK_SET);

		while ((ret = read(fd, buf, MIN(READ_SIZE, remaining))) != 0) {
			std::string temp(buf, ret);
			data += temp;
			remaining -= ret;
			if (remaining == 0)
				break;
		}

		data.resize(len);

		close(fd);

		return data;
	}

	int remove(xcache_meta *meta)
	{
		int rc = -1;
		if (meta) {
			std::string path;

			path = content_dir + meta->store_id();
			rc = unlink(path.c_str());
		}
		return rc;
	}

	void print(void)
	{

	}
};

#endif

#ifndef __DISK_H__
#define __DISK_H__

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <map>
#include "meta.h"

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

	int store(xcache_meta *meta, const std::string *data)
	{
		int ret, fd;
		std::string path;

		path = content_dir + meta->get_cid();

		fd = open(path.c_str(), O_CREAT | O_TRUNC | O_RDWR, 0666);
		if (fd < 0) {
			std::cout << "Could not open file " << path << "\n";
			return -1;
		}

		std::cout << "File " << path << " opened\n";

		ret = 0;

		if (write(fd, data->c_str(), data->length()) !=
		    data->length()) {
			ret = -1;
		}

		close(fd);

		return ret;
	}

	std::string get(xcache_meta *meta)
	{
		int ret, fd, bytes;
		std::string path;
		char buf[256];

		path = content_dir + meta->get_cid();

		fd = open(path.c_str(), O_RDONLY);
		if (fd < 0) {
			std::cout << "Unexpected error.\n";
			return "";
		}

		std::string data("");
		bytes = 0;

		while ((ret = read(fd, buf, 256)) != 0) {
			std::string temp(buf, ret);
			data += temp;
			bytes += ret;
		}

		close(fd);

		return data;
	}

	std::string get_partial(xcache_meta *meta, off_t off, size_t len)
	{
		return "";
	}

	void print(void)
	{

	}
};

#endif

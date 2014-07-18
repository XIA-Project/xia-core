// -*- related-file-name: "../include/click/xiautil.hh" -*-
/*
 */

#include <click/config.h>
#include <click/string.hh>
#include <click/glue.hh>
#include <click/xiautil.hh>
#include <stdlib.h>
#include <unistd.h>

CLICK_DECLS

char *xiaRootDir(char *buf, unsigned len)
{
	char *dir;
	char *pos;

	if (buf == NULL || len == 0)
		return NULL;

	if ((dir = getenv("XIADIR")) != NULL) {
		strncpy(buf, dir, len);
		return buf;
	}
	if (!getcwd(buf, len)) {
		buf[0] = 0;
		return buf;
	}
	pos = strstr(buf, SOURCE_DIR);
	if (pos) {
		pos += sizeof(SOURCE_DIR) - 1;
		*pos = '\0';
	}
	return buf;
}


/*
 * load user defined XIDs for use in parsing and unparsing DAGs
 */
XidMap::XMap XidMap::load()
{
	XidMap::XMap ids;

	char path[PATH_MAX];
	char name[256], text[256];
	short id;
	unsigned len = sizeof(path);
	char *p;

	if ((p = getenv("XIADIR")) != NULL) {
		strncpy(path, p, len);
	} else if (!getcwd(path, len)) {
		path[0] = 0;
	}

	p = strstr(path, SOURCE_DIR);
	if (p) {
		p += sizeof(SOURCE_DIR) - 1;
		*p = '\0';
	}
	strncat(path, "/etc/xids", len);

	FILE *f = fopen(path, "r");
	if (f) {	
		while (!feof(f)) {
			if (fgets(text, sizeof(text), f)) {
 				if (sscanf(text, "%hi %s", &id, name) == 2) {
					ids[id] = name;
				}
			}
		}
		fclose(f);
	}

	return ids;
}

// call the load_xids function at init time to fill in the hash map
XidMap::XMap XidMap::xmap = XidMap::load();

CLICK_ENDDECLS

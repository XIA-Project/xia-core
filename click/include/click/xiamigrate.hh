#ifndef XIAMIGRATE_H
#define XIAMIGRATE_H

#include <click/xiasecurity.hh>
#include <click/xiapath.hh>

// These should really be in clicknet/xia.h or similar
// and also shared with the API.
// Currently, api/include/xia.h has identical definitions
#define EDGES_MAX   4
#define XID_SIZE    20
#define NODES_MAX   20
#define MAX_XID_TYPE_STR 8
#define XIA_XID_STR_SIZE (XID_SIZE*2)+MAX_XID_TYPE_STR
#define XIA_MAX_DAG_STR_SIZE XIA_XID_STR_SIZE*NODES_MAX


// Migrate messages definitions
#define MAX_TIMESTAMP_STR_SIZE 64

// Public functions
bool build_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath &src_path, XIAPath &dst_path);

bool valid_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr);

bool build_migrateack_message(XIASecurityBuffer &migrateack_msg,
        XIAPath &our_addr, XIAPath &their_addr, String timestamp);
#endif // XIA_MIGRATE_H

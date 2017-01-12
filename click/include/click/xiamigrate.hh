#ifndef XIAMIGRATE_H
#define XIAMIGRATE_H

#include <click/xiasecurity.hh>
#include <click/xiapath.hh>
#include <clicknet/xia.h>

// Migrate messages definitions
#define MAX_TIMESTAMP_STR_SIZE 64

// Public functions
bool build_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath &src_path, XIAPath &dst_path, String &migrate_ts);

bool valid_migrate_message(XIASecurityBuffer &migrate_msg,
        XIAPath their_addr, XIAPath our_addr,
        XIAPath &accepted_addr, String &migrate_ts);

bool build_migrateack_message(XIASecurityBuffer &migrateack_msg,
        XIAPath our_addr, XIAPath their_addr, String timestamp);

bool valid_migrateack_message(XIASecurityBuffer &migrateack_msg,
        XIAPath their_addr, XIAPath our_addr, String timestamp);

#endif // XIA_MIGRATE_H

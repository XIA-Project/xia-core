#ifndef _FS_IRQ_TABLE_H_
#define _FS_IRQ_TABLE_H_

#include <mutex>
#include <vector>
#include <unordered_map>

class FSIRQTable {
	public:
		static FSIRQTable *get_table();
		bool add_fetch_request(std::string cid, std::string requestor);
	protected:
		FSIRQTable();
		~FSIRQTable();
	private:
		std::mutex irq_table_lock;
		static FSIRQTable* _instance;

		std::unordered_map<std::string, std::vector<std::string>> _irqtable;
};
#endif //_FS_IRQ_TABLE_H_

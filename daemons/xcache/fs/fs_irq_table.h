#ifndef _FS_IRQ_TABLE_H_
#define _FS_IRQ_TABLE_H_

#include <mutex>
#include <vector>
#include <unordered_map>

typedef std::vector<std::string> RequestorList;
typedef std::unordered_map<std::string, RequestorList> InterestRequestTable;

class FSIRQTable {
	public:
		static FSIRQTable *get_table();
		bool add_fetch_request(std::string cid, std::string requestor);
		RequestorList requestors(std::string cid);
	protected:
		FSIRQTable();
		~FSIRQTable();
	private:
		std::mutex irq_table_lock;
		static FSIRQTable* _instance;

		InterestRequestTable _irqtable;
};
#endif //_FS_IRQ_TABLE_H_

#ifndef _ICID_IRQ_TABLE_H_
#define _ICID_IRQ_TABLE_H_

#include <mutex>
#include <vector>
#include <unordered_map>

typedef std::vector<std::string> RequestorList;
typedef std::unordered_map<std::string, RequestorList> InterestRequestTable;

class ICIDIRQTable {
	public:
		static ICIDIRQTable *get_table();
		bool has_entry(std::string cid);
		bool add_fetch_request(std::string cid, std::string requestor);
		RequestorList requestors(std::string cid);
	protected:
		ICIDIRQTable();
		~ICIDIRQTable();
	private:
		std::mutex irq_table_lock;
		static ICIDIRQTable* _instance;

		InterestRequestTable _irqtable;
};
#endif //_ICID_IRQ_TABLE_H_

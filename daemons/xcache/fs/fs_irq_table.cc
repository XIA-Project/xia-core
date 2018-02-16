#include "fs_irq_table.h"

FSIRQTable* FSIRQTable::_instance = 0;

FSIRQTable::FSIRQTable()
{
	_instance = 0;
}

FSIRQTable* FSIRQTable::get_table()
{
	if(_instance == 0) {
		_instance = new FSIRQTable;
	}
	return _instance;
}

FSIRQTable::~FSIRQTable()
{
	delete _instance;
	_instance = 0;
}

bool FSIRQTable::add_fetch_request(std::string cid, std::string requestor)
{
	// TODO: Sanity check that the requestor is a valid graph
	// TODO: Sanity check that the CID is a content identifier

	// Hold a lock to the table here
	std::lock_guard<std::mutex> lock(irq_table_lock);

	// See if the chunk_id is already in the table
	auto irq = _irqtable.find(cid);

	// If not, create a new table entry
	if(irq == _irqtable.end()) {
		_irqtable[cid] = RequestorList();
	}

	// Add the requestor to the entry
	// TODO: Make sure there are no duplicates in requestor list
	_irqtable[cid].push_back(requestor);

	// If yes, append requestor to the chunk_id entry in table
	return true;
}

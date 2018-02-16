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

RequestorList FSIRQTable::requestors(std::string cid)
{
	RequestorList requestors;

	// Hold a lock to the table
	std::lock_guard<std::mutex> lock(irq_table_lock);

	// Fetch list of all requestors for this chunk
	requestors = _irqtable[cid];

	// Now remove entry for this CID from the table
	// If a caller requests list and is unable to serve every requestor
	// on the list; It is their responsibility to reinstate entries in table
	_irqtable.erase(cid);

	// Remove entry for chunk from the table
	return requestors;
}

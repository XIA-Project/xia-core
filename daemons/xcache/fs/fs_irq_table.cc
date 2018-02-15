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
	// Hold a lock to the table here
	// See if the chunk_id is already in the table
	// If yes, append requestor to the chunk_id entry in table
	// If not, create a new table entry
	return true;
}

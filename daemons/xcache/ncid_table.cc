#include "ncid_table.h"
#include "dagaddr.hpp"


// The only NCIDTable. Initialized by first call to NCIDTable::get_table()
NCIDTable* NCIDTable::_instance = 0;

NCIDTable::NCIDTable()
{
	_instance = 0;
	pthread_rwlock_init(&_rwlock, NULL);
}

/*!
 * @brief The only way to get a reference to the map storing all metadata
 */
NCIDTable* NCIDTable::get_table()
{
	if (_instance == 0) {
		_instance = new NCIDTable;
	}
	return _instance;
}

NCIDTable::~NCIDTable()
{
	std::map<std::string, std::vector<std::string>>::iterator i;

	for (i = _cid_to_ncids.begin(); i != _cid_to_ncids.end(); i++) {
		i->second.clear();
	}
	_cid_to_ncids.clear();
	_ncid_to_cid.clear();

	pthread_rwlock_destroy(&_rwlock);
	delete _instance;
	_instance = 0;
}

/*!
 * @brief register an NCID with the corresponding CID
 *
 * We maintain two maps between NCIDs and CIDs
 * 1. ncid_to_cid - CID corresponding to each NCID known
 * 2. cid_to_ncids - NCIDs that provided CID is associated with
 * TODO: Lock both maps when adding entries
 *
 * @param ncid the NCID to be registered
 * @param cid representing the actual data in storage corresponding to ncid
 *
 * @returns 0 on success, can add existence of ncid in map as failure
 */
int
NCIDTable::register_ncid(std::string ncid, std::string cid)
{
	write_lock();
	_ncid_to_cid_it = _ncid_to_cid.find(ncid);
	_cid_to_ncids_it = _cid_to_ncids.find(cid);

	// NCID is known
	if (_ncid_to_cid_it != _ncid_to_cid.end()) {
		// If the CID is different from the one known before, replace it
		if(_ncid_to_cid_it->second.compare(cid)) {
			printf("Replacing %s for %s\n", cid.c_str(), ncid.c_str());
			_ncid_to_cid[cid] = ncid;
		}
	} else {
		// Simply add to map if not known already
		_ncid_to_cid[ncid] = cid;
	}

	// CID is known
	if (_cid_to_ncids_it != _cid_to_ncids.end()) {
		// Is the NCID already associated with this CID?
		std::vector<std::string> ncids = _cid_to_ncids_it->second;
		std::vector<std::string>::iterator ncids_it;

		// If not, add NCID to the list of NCIDS for this CID
		if(std::find(ncids.begin(), ncids.end(), ncid) == ncids.end()) {
			ncids.push_back(ncid);
		}
	} else {
		// Create the vector and add an entry for this NCID in it
		_cid_to_ncids[cid].push_back(ncid);
	}
	unlock();
	return 0;
}

/*!
 * @brief unregister an NCID with the corresponding CID
 *
 * We maintain two maps between NCIDs and CIDs
 * 1. ncid_to_cid - CID corresponding to each NCID known
 * 2. cid_to_ncids - NCIDs that provided CID is associated with
 * TODO: Lock both maps when adding entries
 *
 * @param ncid the NCID to be unregistered
 * @param cid corresponding to the NCID that is getting unregistered
 *
 * @returns 0 on success
 * @returns -1 if the entries to be unregistered don't exist
 */

int
NCIDTable::unregister_ncid(std::string ncid, std::string cid)
{
	int retval = -1;
	std::vector<std::string> ncids;
	std::vector<std::string>::iterator ncids_it;

	write_lock();

	_ncid_to_cid_it = _ncid_to_cid.find(ncid);
	_cid_to_ncids_it = _cid_to_ncids.find(cid);
	if (_ncid_to_cid_it == _ncid_to_cid.end()) {
		printf("unregister_ncid: ERROR NCID %s not found\n", ncid.c_str());
		goto unregister_ncid_done;
	}

	if(_cid_to_ncids_it == _cid_to_ncids.end()) {
		printf("unregister_ncid: ERROR NCIDs missing for %s\n", cid.c_str());
		goto unregister_ncid_done;
	}

	// Erase from ncid_to_cid map
	_ncid_to_cid.erase(_ncid_to_cid_it);

	// Erase NCID from list of NCIDs for given CID
	// TODO: Is this really erasing from the vector or a copy of the vector?
	ncids = _cid_to_ncids_it->second;
	ncids_it = std::find(ncids.begin(), ncids.end(), ncid);
	ncids.erase(ncids_it);

	retval = 0; // Atomically erased entries from both maps

unregister_ncid_done:
	unlock();
	return retval;

}

/*!
 * @brief Given an NCID or CID return the corresponding CID
 *
 * If the given argument is CID, it is returned as is, only if it has
 * NCIDs associated with it.
 *
 * If the given argument is NCID, the corresponding CID is returned
 * if it exists in the tables.
 *
 * @param content_id
 * @returns "" on error
 * @returns CID as a string on success
 */
std::string
NCIDTable::get_known_cid(std::string content_id)
{
	std::string cid = "";

	read_lock();

	Node content_id_node(content_id);

	// Return CID corresponding to requested NCID
	if (content_id_node.type() == CLICK_XIA_XID_TYPE_NCID) {
		_ncid_to_cid_it = _ncid_to_cid.find(content_id);
		if(_ncid_to_cid_it != _ncid_to_cid.end()) {
			cid = _ncid_to_cid_it->second;
		}
		goto get_known_cid_done;
	}

	// Return CID corresponding to requested CID, if it is known
	_cid_to_ncids_it = _cid_to_ncids.find(content_id);
	if(_cid_to_ncids_it != _cid_to_ncids.end()) {
		cid = content_id;
		goto get_known_cid_done;
	}

get_known_cid_done:
	unlock();
	return cid;
}

/*!
 * @brief Convert given CID/NCID to CID
 *
 * if a CID is given, return as is, without any verification
 * If an NCID is given, find the corresponding CID and return that
 *
 * @param content_id CID or NCID to be converted to CID
 * @returns CID if a CID or a known NCID is given
 * @returns "" if an unknown NCID was given
 */
std::string
NCIDTable::to_cid(std::string content_id)
{
	std::string cid = "";

	// Sanity check the content_id. Should be at least 40 bytes
	if(content_id.size() < (CLICK_XIA_XID_ID_LEN*2)) {
		return "";
	}
	Node content_id_node(content_id);

	read_lock();

	// Find CID if the content_id was an NCID
	if(content_id_node.type() == CLICK_XIA_XID_TYPE_NCID) {
		_ncid_to_cid_it = _ncid_to_cid.find(content_id);
		if(_ncid_to_cid_it != _ncid_to_cid.end()) {
			cid = _ncid_to_cid_it->second;
		}
		goto to_cid_done;
	}

	// Return CID as is, if content_id was a CID
	if(content_id_node.type() == CLICK_XIA_XID_TYPE_CID) {
		cid = content_id;
		goto to_cid_done;
	}

to_cid_done:
	unlock();
	return cid;
}

int
NCIDTable::unregister_cid(std::string cid)
{
	int retval = -1;
	std::vector<std::string> *ncids;
	std::vector<std::string>::iterator ncids_it;

	read_lock();

	// Was the given argument a CID?
	Node cid_node(cid);
	if (cid_node.type() != CLICK_XIA_XID_TYPE_CID) {
		goto unregister_cid_done;
	}

	// Do we have the CID in cid_to_ncids table?
	_cid_to_ncids_it = _cid_to_ncids.find(cid);
	if (_cid_to_ncids_it == _cid_to_ncids.end()) {
		printf("unregister_cid: CID not in table\n");
		goto unregister_cid_done;
	}

	// Delete all NCIDs that point to this CID
	ncids = &_cid_to_ncids_it->second;
	for(ncids_it = ncids->begin();ncids_it !=ncids->end();ncids_it++) {
		_ncid_to_cid_it = _ncid_to_cid.find(*ncids_it);
		if(_ncid_to_cid_it != _ncid_to_cid.end()) {
			_ncid_to_cid.erase(_ncid_to_cid_it);
		}
	}
	// Then delete the CID entry that pointed to list of NCIDs
	ncids->clear();
	_cid_to_ncids.erase(_cid_to_ncids_it);

	retval = 0;
unregister_cid_done:
	unlock();
	return retval;
}

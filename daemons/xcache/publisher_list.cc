#include "publisher_list.h"
#include "dagaddr.hpp"


// The only PublisherList. Initialized by first call to PublisherList::get_map()
PublisherList* PublisherList::_instance = 0;

PublisherList::PublisherList()
{
	_instance = 0;
	pthread_rwlock_init(&_rwlock, NULL);
}

/*!
 * @brief Get a reference to a publisher by its name
 *
 * TODO: In future we should throw exception if the publisher creds
 * are not found during object creation.
 *
 * Return a reference to a Publisher instance. If one doesn't exist
 * it is created.
 *
 * If the private key for the publisher is present, this instance can
 * sign and publish named content. The clients will be able to trust
 * this content by verifying publisher cert against CA root certificate.
 *
 * On a client, this Publisher instance will be able to fetch the
 * publisher's certificate and verify the named content against it.
 *
 * If a Publisher instance already exists, it is returned as is.
 *
 * @param publisher_name the string used to represent the publisher
 *
 * @returns a reference to the requested Publisher object
 */
Publisher *
PublisherList::get(std::string publisher_name)
{
	Publisher *publisher;

	write_lock();

	// Get a reference to the publisher or create a new one
	_name_to_publisher_it = _name_to_publisher.find(publisher_name);
	if (_name_to_publisher_it == _name_to_publisher.end()) {
		// Create a new Publisher instance
		publisher = new Publisher(publisher_name);
		_name_to_publisher[publisher_name] = publisher;
	} else {
		publisher = _name_to_publisher_it->second;
	}

	unlock();
	return publisher;
}

/*!
 * @brief The only way to get a reference to the list of publishers
 */
PublisherList* PublisherList::get_publishers()
{
	if (_instance == 0) {
		_instance = new PublisherList;
	}
	return _instance;
}

PublisherList::~PublisherList()
{
	// Delete all Publisher objects from heap
	for(_name_to_publisher_it = _name_to_publisher.begin();
			_name_to_publisher_it != _name_to_publisher.end();
			_name_to_publisher_it++) {
		delete _name_to_publisher_it->second;
	}
	// Clear out the map
	_name_to_publisher.clear();

	pthread_rwlock_destroy(&_rwlock);
	delete _instance;
	_instance = 0;
}


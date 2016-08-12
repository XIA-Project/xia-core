#ifndef __LRU_H__
#define __LRU_H__

#include <stdint.h>
#include "policy.h"

class LruPolicy:public xcache_eviction_policy {	

	// implement LRU policy using doubly linked list 
	typedef struct _Node{
		xcache_meta* meta;
		_Node* prev;
		_Node* next;
	} Node;

	Node* head;
	Node* tail;

	uint64_t size;					// current total chunk size of the cache in bytes
	uint64_t capacity;				// capacity is in bytes

	// speed up the node lookup for a given meta data
	std::map<xcache_meta*, Node*> meta2Node;

public:
	LruPolicy(uint64_t capacity) {
		this->head = NULL;
		this->tail = NULL;
		this->size = 0;

		this->capacity = capacity;			
	}

	~LruPolicy(){
		Node* ptr = this->head;

		while(ptr != NULL){
			Node* nextPtr = ptr->next;
			delete ptr;
			ptr = nextPtr;
		}
	}

	/**
	 * try to store a given meta into the cache, iff there is enough space and 
	 * we haven't seen this meta before (so no duplicate in the list)
	 */
	int store(xcache_meta *meta) {
		// if we haven't seen this meta before 
		// note meta must have unique address (using new to allocate)
		if(meta2Node.find(meta) != meta2Node.end()){
			return -1;
		}

		// check if there is enough size, taking into account if this store 
		// will eventually result in a eviction
		uint64_t newSize = this->size + meta->get_length();
		if(newSize > this->capacity && this->head != NULL && this->tail != NULL){
			newSize -= this->tail->meta->get_length();
		}

		// can't store this chunk, too big
		if(newSize > this->capacity){
			return -1;
		}

		Node* newNode = new Node;
		newNode->meta = meta;
		newNode->prev = NULL;

		meta2Node[meta] = newNode;
		
		// if this is the first node, insert it
		if(!this->head && !this->tail){
			newNode->next = NULL;

			this->head = newNode;
			this->tail = newNode;
		}
		// if this is not the first node, put it at the first node
		else {
			newNode->next = this->head;
			this->head->prev = newNode;
			this->head = newNode;
		}

		// total CIDs in cache increase
		this->size += meta->get_length();

		return 1;
	}

	/**
	 * try to get the meta in the Cache; succeed only if meta is in the cache
	 * Also move the item to head of the list since we just accessed it
	 */
	int get(xcache_meta *meta) {
		// check the validity of the cache
		if(!this->head || !this->tail){
			return -1;
		}

		// if we haven't seen this meta, return
		if(meta2Node.find(meta) == meta2Node.end()){
			return -1;
		}

		// get the node corresponding to this meta
		Node* ptr = meta2Node[meta];

		// if this points to head, we are done already
		if(ptr == this->head){
			return 1;
		}

		// if this is a tail node, set the tail to previous node
		if(this->tail == ptr){
			this->tail = ptr->prev;
		}

		// fix neighbor's pointer appropriately
		if(ptr->prev){
			ptr->prev->next = ptr->next;
		}
		if(ptr->next){
			ptr->next->prev = ptr->prev;
		}

		// move to head
		ptr->next = this->head;
		this->head->prev =ptr;
		this->head = ptr;

		return 1;
	}

	/**
	 * remove a meta from a cache, fix pointers etc
	 */
	int remove(xcache_meta *meta) {
		// check validity of the cache
		if(!this->head || !this->tail){
			return -1;
		}

		// no meta, don't remove
		if(meta2Node.find(meta) == meta2Node.end()){
			return -1;
		}

		// fix node pointer's corresponding to this meta
		// since we are removing it
		Node* ptr = meta2Node[meta];
		if(ptr->prev){
			ptr->prev->next = ptr->next;
		}
		if(ptr->next){
			ptr->next->prev = ptr->prev;
		}

		// fix the head and tail pointers
		if(ptr == this->head && ptr == this->tail){
			this->head = NULL;
			this->tail = NULL;
		} else if(ptr == this->head){
			this->head = ptr->next;
		} else if (ptr == this->tail){
			this->tail = ptr->prev;
		}

		// do the necessary cleanup
		this->size -= meta->get_length();
		meta2Node.erase(meta);
		delete ptr;

		return 1;
	}

	/**
	 * evict a meta from the cache if the current cache size is higher
	 * than capacity
	 */
	xcache_meta *evict() {
		// nothing to evict since size is not larger
		if(this->size <= this->capacity){
			return NULL;
		}

		if(!this->tail){
			return NULL;
		}

		// evict the least recent node (tail) and fix tail's neighbor's pointer
		Node* leastRecentNode = this->tail;
		if(leastRecentNode->prev){
			leastRecentNode->prev->next = NULL;
		}

		// fix head and tail pointers
		if(this->head == leastRecentNode){
			this->head = NULL;
			this->tail = NULL;
		} else {
			this->tail = leastRecentNode->prev;
		}

		// some clean up
		xcache_meta* retMeta = leastRecentNode->meta;
		this->size -= retMeta->get_length();
		meta2Node.erase(retMeta);
		delete leastRecentNode;

		return retMeta;
	}
};

#endif

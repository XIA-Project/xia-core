#ifndef __LRU_H__
#define __LRU_H__

#include <stdint.h>
#include "policy.h"

class LruPolicy:public xcache_eviction_policy {	
	typedef struct _Node{
		xcache_meta* meta;
		_Node* prev;
		_Node* next;
	} Node;

	Node* head;
	Node* tail;
	uint64_t size;
	uint64_t capacity;

	std::map<xcache_meta*, Node*> meta2Node;

public:
	LruPolicy(uint64_t capacity) {
		this->head = NULL;
		this->tail = NULL;
		this->size = 0;

		this->capacity = capacity;			// capacity is in bytes
	}

	~LruPolicy(){
		Node* ptr = this->head;

		while(ptr != NULL){
			Node* nextPtr = ptr->next;
			delete ptr;
			ptr = nextPtr;
		}
	}

	int store(xcache_meta *meta) {
		if(meta2Node.find(meta) != meta2Node.end()){
			return -1;
		}

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
		// if this is not the first node
		else {
			newNode->next = this->head;
			this->head->prev = newNode;
			this->head = newNode;
		}

		this->size += meta->get_length();

		return 1;
	}

	int get(xcache_meta *meta) {
		if(!this->head || !this->tail){
			return -1;
		}

		if(meta2Node.find(meta) == meta2Node.end()){
			return -1;
		}

		Node* ptr = meta2Node[meta];
		if(ptr == this->head){
			return 1;
		}
		if(this->tail == ptr){
			this->tail = ptr->prev;
		}

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

	int remove(xcache_meta *meta) {
		if(!this->head || !this->tail){
			return -1;
		}

		if(meta2Node.find(meta) == meta2Node.end()){
			return -1;
		}

		Node* ptr = meta2Node[meta];
		if(ptr->prev){
			ptr->prev->next = ptr->next;
		}
		if(ptr->next){
			ptr->next->prev = ptr->prev;
		}

		if(ptr == this->head && ptr == this->tail){
			this->head = NULL;
			this->tail = NULL;
		} else if(ptr == this->head){
			this->head = ptr->next;
		} else if (ptr == this->tail){
			this->tail = ptr->prev;
		}

		this->size -= sizeof(meta);
		meta2Node.erase(meta);
		delete ptr;

		return 1;
	}

	xcache_meta *evict() {
		// nothing to evict
		if(this->size <= this->capacity){
			return NULL;
		}

		if(!this->tail){
			return NULL;
		}

		Node* leastRecentNode = this->tail;
		if(leastRecentNode->prev){
			leastRecentNode->prev->next = NULL;
		}

		if(this->head == leastRecentNode){
			this->head = NULL;
			this->tail = NULL;
		} else {
			this->tail = leastRecentNode->prev;
		}

		xcache_meta* retMeta = leastRecentNode->meta;
		this->size -= retMeta->get_length();
		meta2Node.erase(retMeta);
		delete leastRecentNode;

		return retMeta;
	}
};

#endif

#ifndef _ICID_WORK_REQUEST_H
#define _ICID_WORK_REQUEST_H

/*!
 * @brief Abstract class representing Forwarding Service work request
 *
 * All tasks in the Forwarding Service are queued up in a WorkQueue
 * and are then scheduled among Worker threads.
 */

class ICIDWorkRequest {
	public:
		virtual ~ICIDWorkRequest() {}
		virtual void process() = 0;
	protected:

};
#endif // _WORK_REQUEST_H

/*
 * taskident - an identifiable task. Basically adds an id to 
 * the base Task class.
 */

#ifndef CLICK_TASKIDENT_HH
#define CLICK_TASKIDENT_HH
#include <click/task.hh>
#include <click/element.hh>

#include <cstdint> // uint32_t

CLICK_DECLS

class TaskIdent : public Task{ 

public:

    inline TaskIdent(Element *e, uint32_t id) : Task(e), _id(id){ };
    inline TaskIdent(TaskCallback f, void *user_data, uint32_t id) : 
        Task(f, user_data), _id(id){ };
    
    ~TaskIdent() {}; // Note that the system may crash if we attempt to destroy
                     // the object from a base class pointer. If we end having
                     // to support that, the base class (Task) destructor must
                     // be made virtual.

    inline uint32_t get_id() { return _id; };

  private:
    uint32_t _id;
};

CLICK_ENDDECLS
#endif

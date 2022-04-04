//
// Created by chucklind on 04/04/2022.
//

#include "uthreads.h"
#include "thread.h"

Thread::Thread(const unsigned int id, const thread_entry_point entry_point, const int sp)
    : id(id),
    entry_point(entry_point),
    sp(sp)
    {}

void Thread::set_state(State state){
    this->state = state;
    }

unsigned int Thread::get_id() const{
    return this->id;
    }

thread_entry_point Thread::get_entry_point() const{
    return this->entry_point;
    }

State Thread::get_state() const{
    return this->state;
    }


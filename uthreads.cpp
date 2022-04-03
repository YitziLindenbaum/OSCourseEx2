//
// Created by chucklind on 29/03/2022.
//

#include "uthreads.h"

enum State {READY, RUNNING, BLOCKED};


class Thread {

private:
    unsigned int id;
    thread_entry_point entry_point;
    State state;
    unsigned int sp;

public:

    Thread(const unsigned int id, const thread_entry_point entry_point, const int sp) {
        this->id = id;
        this->entry_point = entry_point;
        this->sp = sp;
    }

    void set_state(State state){
        this->state = state;
    }

    unsigned int get_id() const{
        return this->id;
    }

    thread_entry_point get_entry_point() const{
        return this->entry_point;
    }

    State get_state() const{
        return this->state;
    }

};

int uthread_init(int quantum_usecs) {
    return 0;
}


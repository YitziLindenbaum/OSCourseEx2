//
// Created by chucklind on 04/04/2022.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#include <setjmp.h>

enum State {READY, RUNNING, BLOCKED};
typedef unsigned int id_t;

class Thread {
private:
    id_t id;
    thread_entry_point initial_entry_point;
    char* initial_sp;
    State state;

    jmp_buf env;

public:

    Thread(unsigned int id, thread_entry_point entry_point);
    ~Thread();

    void run();

    int die();

    void set_state(State state);

    id_t get_id() const;

    thread_entry_point get_init_entry_point() const;

    State get_state() const;

    jmp_buf &get_env();


};





#endif //EX2_THREAD_H

//
// Created by chucklind on 04/04/2022.
//

#ifndef EX2_THREAD_H
#define EX2_THREAD_H

enum State {READY, RUNNING, BLOCKED};

class Thread {
private:
    unsigned int id;
    thread_entry_point entry_point;
    State state;
    unsigned int sp;

public:

    Thread(unsigned int id, thread_entry_point entry_point, int sp);

    void set_state(State state);

    unsigned int get_id() const;

    thread_entry_point get_entry_point() const;

    State get_state() const;

};





#endif //EX2_THREAD_H

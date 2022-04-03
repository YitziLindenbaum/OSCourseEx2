//
// Created by chucklind on 29/03/2022.
//

#include <queue>
#include <unordered_set>
#include <set>
#include "uthreads.h"
using namespace std;

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

class Scheduler {
    queue<Thread> ready;
    set<Thread> blocked;
    Thread running;
    unsigned int live_threads = 0;
    set<unsigned int> available_ids;

    int spawn(thread_entry_point entry_point) {
        unsigned int next_id = min(live_threads + 1, *(available_ids.begin()));
        Thread new_thread = new Thread(next_id, entry_point, placeholder);
    }
};

int uthread_init(int quantum_usecs) {
    return 0;
}


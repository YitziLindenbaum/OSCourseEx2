//
// Created by chucklind on 29/03/2022.
//

#include <queue>
#include <unordered_set>
#include <set>
#include <map>
#include "uthreads.h"
#include "thread.h"
using namespace std;
typedef unsigned int id_t;

class Scheduler {
    unsigned int quantum_usecs;
    queue<Thread> ready;
    set<Thread> blocked;
    Thread* running;
    set<id_t> available_ids;
    map<id_t, Thread> all_threads;

    Scheduler(unsigned int quantum_usecs)
    : quantum_usecs(quantum_usecs) {
        for (id_t i = 1; i < MAX_THREAD_NUM; ++i) {available_ids.insert(i);}
        // @todo set all_threads[0] to be main thread
    }

    int spawn(thread_entry_point entry_point) {
        if (available_ids.empty()) {
            return -1; // maximum number of threads exceeded
        }

        id_t next_id = *(available_ids.begin());
        Thread* new_thread = new Thread(next_id, entry_point, ); //@todo stack-pointer arg (use env?)
    }
};

map<unsigned int, Thread> *thread_map = new map<unsigned int, Thread>();
//thread_map[0] = new Thread(0, );

int uthread_init(int quantum_usecs) {
    return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
    unsigned int new_id;
    thread_map[new_id] = new Thread(new_id, entry_point, )
    return 0;
}

int uthread_terminate(int tid) {
    return 0;
}


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
    queue<id_t> ready;
    set<id_t> blocked;
    id_t running;
    set<id_t> available_ids;
    map<id_t, Thread> thread_map;

    Scheduler(unsigned int quantum_usecs)
    : quantum_usecs(quantum_usecs), running(0)
    {
        for (id_t i = 1; i < MAX_THREAD_NUM; ++i) {available_ids.insert(i);}
        // @todo set thread_map[0] to be main thread
    }

    int spawn(thread_entry_point entry_point) {
        // validate entry_point != NULL
        if (available_ids.empty()) {
            return -1; // maximum number of threads exceeded
        }

        id_t next_id = *(available_ids.begin());
        char* stack = new char[STACK_SIZE];
        Thread new_thread = *(new Thread(next_id, entry_point));
        ready.push(next_id);
        thread_map[next_id] = new_thread;
        return 0;
    }


    void switchToThread(id_t tid) {
        int ret_val = sigsetjmp(thread_map[running].get_env(), 1);
        if (!ret_val) {
            running = tid;
            thread_map[running].run();
        }
    }

    int terminateThread(id_t tid) { // assumes that tid is positive
        //validate that tid exists in thread_map @todo address case where thread commits suicide
        Thread to_kill = thread_map[tid];
        thread_map.erase(tid);
        delete &to_kill;
        // @todo continue coding here
    }
};


/*
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
 */


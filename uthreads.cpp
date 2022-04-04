//
// Created by chucklind on 29/03/2022.
//

#include <queue>
#include <unordered_set>
#include <set>
#include "uthreads.h"
using namespace std;

class Scheduler {
    unsigned int quantum_usecs;
    queue<Thread> ready;
    set<Thread> blocked;
    Thread running;
    unsigned int live_threads = 0;
    set<unsigned int> available_ids;

    int spawn(thread_entry_point entry_point) {
        unsigned int next_id = min(live_threads + 1, *(available_ids.begin()));
        Thread* new_thread = new Thread(next_id, entry_point, placeholder);
    }
};

int uthread_init(int quantum_usecs) {
    return 0;
}


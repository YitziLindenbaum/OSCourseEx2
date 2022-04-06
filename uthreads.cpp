//
// Created by chucklind on 29/03/2022.
//

#include <list>
#include <unordered_set>
#include <set>
#include <map>
#include <iostream>
#include "uthreads.h"
#include "thread.h"

#define DEAD_THREAD "Thread does not exist"
#define MAIN_THREAD_BLOCK "Main thread cannot be blocked"
#define MAIN_ID 0
using namespace std;
typedef unsigned int id_t;


class Scheduler {
    unsigned int quantum_usecs;
    list<id_t> ready;
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
        ready.push_back(next_id);
        thread_map[next_id] = new_thread;
        return 0;
    }

    bool is_alive(id_t tid){
        return thread_map.count(tid); // 1 if alive, 0 if not
    }

    int switch_to_thread(id_t tid) {
      if (!is_alive(tid)){
          std:cerr << DEAD_THREAD << std::endl;
          return -1;
      }
      if (is_alive(running)) {
        int ret_val = sigsetjmp(thread_map[running].get_env(), 1);
        if (ret_val) {return 0;}
      }

      ready.push_back(running);
      running = tid;
      thread_map[running].run();
      return 0;
    }

    int terminate_thread(id_t tid) { // assumes that tid is positive
        if (!is_alive(tid)) {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        Thread to_kill = thread_map[tid];
        thread_map.erase(tid);
        delete &to_kill;

        if(running == tid){
          return switch_to_next();
        }
        return 0;
    }

    int switch_to_next() {
        /**
         * @brief: switches to next thread in READY and removes it from READY
         * @return 0 upon success, -1 upon failure
         * @todo what if READY is empty?
         */
        id_t to_switch = ready.front();
        ready.pop_front();
        return switch_to_thread(to_switch);
    }

    int block(id_t tid) {
        if (!is_alive(tid)) {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        if (tid == MAIN_ID) {
            std::cerr << MAIN_THREAD_BLOCK << std::endl;
            return -1;
        }
        ready.remove(tid);
        blocked.insert(tid);
        if (tid == running) {
            switch_to_next();
        }
        return 0;
    }

    int resume (id_t tid) {
        if (!is_alive(tid)) {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        if (blocked.count(tid)) { // if not in blocked, tid is either in READY or RUNNING and func should have no
            // effect
            blocked.erase(tid);
            ready.push_back(tid);
        }
        return 0;
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


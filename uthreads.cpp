//
// Created by chucklind on 29/03/2022.
//

#include <list>
#include <unordered_set>
#include <set>
#include <map>
#include <iostream>
#include <signal.h>
#include <sys/time.h>
#include "uthreads.h"
#include "thread.h"

#define DEAD_THREAD "Thread does not exist"
#define MAIN_THREAD_BLOCK "Main thread cannot be blocked"
#define MAIN_ID 0
using namespace std;
typedef unsigned int id_t;


class Scheduler {
    int quantum_usecs;
    static int elapsed_quantums;
    list<id_t> ready;
    set<id_t> blocked;
    id_t running;
    set<id_t> available_ids;
    map<id_t, Thread> thread_map;
    struct itimerval timer;

public:
    Scheduler(int quantum_usecs)
    : quantum_usecs(quantum_usecs), running(0)
    {
        for (id_t i = 1; i < MAX_THREAD_NUM; ++i) {available_ids.insert(i);}
        // @todo set thread_map[0] to be main thread
        //Thread main_thread(0, NULL);
       // thread_map.emplace(0, main_thread);

        timer.it_value.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_value.tv_sec = quantum_usecs % 1000000;
        timer.it_interval.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_interval.tv_sec = quantum_usecs % 1000000;
        setitimer(ITIMER_VIRTUAL, &timer, NULL);
    }


    int spawn(thread_entry_point entry_point) {
        // validate entry_point != NULL
        if (available_ids.empty()) {
            return -1; // maximum number of threads exceeded
        }

        id_t next_id = *(available_ids.begin());
        char* stack = new char[STACK_SIZE];
        Thread new_thread(next_id, entry_point);
        ready.push_back(next_id);
        thread_map.emplace(next_id, new_thread);
        return 0;
    }

    bool is_alive(id_t tid){
        return thread_map.count(tid); // 1 if alive, 0 if not
    }


    int terminate_thread(id_t tid) { // assumes that tid is positive
        if (!is_alive(tid)) {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        Thread& to_kill = thread_map.at(tid);
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
         * @todo what if READY is empty? (answer: if READY is empty, then the main thread is running. So should not happen
         */
        if (ready.empty()){return 0;} // main thread is running and ready is empty - continue running

        id_t to_switch = ready.front();
        ready.pop_front();
        if (!is_alive(to_switch)){
            std:cerr << DEAD_THREAD << std::endl;
            return -1;
        }

        if (!blocked.count(running)) { // make sure current thread has not just blocked itself
            ready.push_back(running);
        }

        if (is_alive(running)) { // save state of current thread before switching
            int ret_val = sigsetjmp(thread_map.at(running).get_env(), 1);
            if (ret_val) {return 0;}
        }

        running = to_switch;
        thread_map.at(running).run();
        return 0;

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
            return switch_to_next(); // this probably shouldn't return (since it doesn't affect whether block is successful)
            // probably better to somehow perform check...
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

    id_t get_running_id(){
        return running;
    }


};

int gotit = 0;

void timer_handler(int sig) {
    gotit = 1;
    std::cout << "Caught the timer" << std::endl;
}

int main() {
    struct sigaction sa = {0};

    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        printf("sigaction error.");
    }
    Scheduler scheduler = Scheduler(100);
    for(;;){
        if (gotit) {
            std::cout << "got it" << std::endl;
        }
    }
}


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


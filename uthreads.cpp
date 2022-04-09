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
    int elapsed_quantums;
    list<id_t> ready;
    set<id_t> blocked;
    id_t running;
    set<id_t> available_ids;
    map<id_t, Thread> thread_map;
    struct itimerval timer;

public:
    Scheduler(int quantum_usecs)
    : quantum_usecs(quantum_usecs), running(0),elapsed_quantums(0)
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

    ~Scheduler()= default;


    int spawn(thread_entry_point entry_point) {
        if (entry_point == nullptr){
          return -1;
        }
        if (available_ids.empty()) {
            return -1; // maximum number of threads exceeded
        }

        id_t next_id = *(available_ids.begin());
        char* stack = new char[STACK_SIZE];
        Thread new_thread(next_id, entry_point);
        ready.push_back(next_id);
        thread_map.emplace(next_id, new_thread);
        return next_id;
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
        blocked.erase(tid);
        ready.remove(tid);
        delete &to_kill;

        if(running == tid){
          running = ready.front();
          ready.pop_front();
          thread_map[running].run();
          thread_map[running].set_state(RUNNING);
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
        //@todo check if necessary because there should not be a dead thread in the ready queue.
        // (if does, it probably our mistake and not hte user's, so we dont need to use cerr.
        if (!is_alive(to_switch)){
            std:cerr << DEAD_THREAD << std::endl;
            return -1;
        }

        if (!blocked.count(running)) { // make sure current thread has not just blocked itself
            ready.push_back(running);
            thread_map[running].set_state(READY);
        }

        if (is_alive(running)) { // save state of current thread before switching
            int ret_val = sigsetjmp(thread_map.at(running).get_env(), 1);
            if (ret_val) {return 0;}
        }

        running = to_switch;
        thread_map[running].run();
        thread_map[running].set_state(RUNNING);
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
        thread_map[running].set_state(BLOCKED);
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
      // if not in blocked, tid is either in READY or RUNNING and func should have no effect
        if (blocked.count(tid)) {
            blocked.erase(tid);
            ready.push_back(tid);
            thread_map[running].set_state(READY);
        }
        return 0;
    }

    int sleep(int num_quantum){

    }

    id_t get_running_id(){
        return running;
    }

    int get_quatums(){
      return elapsed_quantums;
    }

    Thread get_thread(id_t tid){
      if(!thread_map.count(tid)){

      }
      return thread_map[tid];
    }


};





static Scheduler scheduler = NULL;



int uthread_init(int quantum_usecs){
  if(quantum_usecs <= 0){
    return -1;
  }
  scheduler = Scheduler(quantum_usecs);
  return 0;
}

int uthread_spawn(thread_entry_point entry_point){
  return scheduler.spawn(entry_point);
}

int uthread_terminate(int tid){
  if(tid == 0){
    delete &scheduler;
    exit(0);
  }
  return scheduler.terminate_thread(tid);
}

int uthread_block(int tid){
  return scheduler.block(tid);
}

int uthread_resume(int tid){
  return scheduler.resume(tid);
}

int uthread_sleep(int num_quantums){

}

int uthread_get_tid(){

}

int uthread_get_total_quantums(){
  return scheduler.get_quatums();
}

int uthread_get_quantums(int tid){

  return scheduler.get_thread(tid).get_num_quantums();
}


























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


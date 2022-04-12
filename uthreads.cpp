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

#define SYS_ERR "system error: "
#define TIMER_ERR SYS_ERR << "failed to set timer"
#define SIGNAL_ERR SYS_ERR << "failed to set signal handler"
#define LIB_ERR "thread library error: "
#define DEAD_THREAD LIB_ERR << "thread does not exist"
#define MAIN_THREAD_BLOCK LIB_ERR << "main thread cannot be blocked"
#define NON_POS LIB_ERR << "quantum must be a positive integer"
#define NON_POS_NUMQ LIB_ERR << "Number of quantums must be a positive integer"
#define MAIN_ID 0
using namespace std;
typedef unsigned int id_t;


class Scheduler
{
    int quantum_usecs;
    int elapsed_quantums;
    list<id_t> ready;
    set<id_t> blocked;
    id_t running;
    set<id_t> available_ids;
    map<id_t, int> sleeping;
    map<id_t, Thread> thread_map;

public:
    Scheduler(int quantum_usecs)
            : quantum_usecs(quantum_usecs), running(0), elapsed_quantums(0) // elapsed_quantums should be 1 at start?
    {
        for (id_t i = 1; i < MAX_THREAD_NUM; ++i)
        { available_ids.insert(i); }
        Thread main_thread(0, NULL);
        thread_map.emplace(0, main_thread);

    }

    ~Scheduler() = default;
    /*
    ~Scheduler() {
        delete &thread_map.at(0);
    };
    */

    int spawn(thread_entry_point entry_point)
    {
        if (entry_point == nullptr)
        {
            return -1;
        }
        if (available_ids.empty())
        {
            return -1; // maximum number of threads exceeded
        }

        id_t next_id = *(available_ids.begin());
        char *stack = new char[STACK_SIZE];
        Thread new_thread(next_id, entry_point);
        ready.push_back(next_id);
        thread_map.emplace(next_id, new_thread);
        return next_id;
    }

    bool is_alive(id_t tid)
    {
        return thread_map.count(tid); // 1 if alive, 0 if not
    }


    int terminate_thread(id_t tid)
    { // assumes that tid is positive
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }

        Thread &to_kill = thread_map.at(tid);
        thread_map.erase(tid);
        blocked.erase(tid);
        ready.remove(tid); // this may not be necessary since we check if the next ready thread is alive anyway, and
        // searching a linked list is inefficient
        delete &to_kill;

        if (running == tid) // @todo can replace with call to switch_to_next?
        {
            running = ready.front();
            ready.pop_front();
            thread_map.at(running).set_state(RUNNING);
            thread_map.at(running).run();
        }
        return 0;
    }

    void switch_to_next(bool add_to_ready=false)
    {
        if (ready.empty())
        { return; } // main thread is running and ready is empty - continue running

        id_t to_switch;
        do { // pop from READY until a live thread is found
            to_switch = ready.front();
            ready.pop_front();
        } while(!is_alive(to_switch));


        if (add_to_ready) // place running thread in READY iff it has not just blocked itself
        {
            ready.push_back(running);
            thread_map.at(running).set_state(READY);
        }

        if (is_alive(running))
        { // save state of current thread before switching
            int ret_val = sigsetjmp(thread_map.at(running).get_env(), 1);
            if (ret_val)
            { return; }
        }

        running = to_switch;
        thread_map.at(running).set_state(RUNNING);
        thread_map.at(running).run();

    }

    int block(id_t tid)
    {
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        if (tid == MAIN_ID)
        {
            std::cerr << MAIN_THREAD_BLOCK << std::endl;
            return -1;
        }
        ready.remove(tid);
        blocked.insert(tid);
        thread_map.at(running).set_state(BLOCKED);
        if (tid == running)
        {
            switch_to_next();
        }
        return 0;
    }

    int resume(id_t tid)
    {
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }
        // if not in blocked, tid is either in READY or RUNNING and func should have no effect
        if (blocked.count(tid))
        {
            blocked.erase(tid);
            if (!sleeping.count(tid)) {
                ready.push_back(tid);
                thread_map.at(tid).set_state(READY);
            }
        }
        return 0;
    }

    int sleep(int num_quantums)
    {
        if (num_quantums <= 0) {
            std::cerr << NON_POS_NUMQ << std::endl;
            return -1;
        }
        if (running == MAIN_ID) {
            std::cerr << MAIN_THREAD_BLOCK << std::endl;
            return -1;
        }

        sleeping.emplace(running, num_quantums);
        switch_to_next();
        return 0;
    }

    void update_and_wake() { // must be called by timer handler
        for (auto it = sleeping.begin(); it != sleeping.end(); ++it) {
            id_t tid = it->first;
            if (--sleeping[tid] == 0) {
                sleeping.erase(tid);
                if (is_alive(tid) && !(blocked.count(tid))) {
                    ready.push_back(tid);
                    thread_map.at(tid).set_state(READY);
                }
            }
        }
    }

    id_t get_running_id()
    {
        return running;
    }

    int get_quantums()
    {
        return elapsed_quantums;
    }

    Thread get_thread(id_t tid)
    {
        if (!thread_map.count(tid))
        {

        }
        return thread_map.at(tid);
    }


};


void setup_timer(int quantum_usecs, struct itimerval &timer) {
    timer.it_value.tv_sec = quantum_usecs / (int) 1000000;
    timer.it_value.tv_usec = quantum_usecs % 1000000;
    timer.it_interval.tv_sec = quantum_usecs / (int) 1000000;
    timer.it_interval.tv_usec = quantum_usecs % 1000000;
    if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {
        std::cerr << TIMER_ERR << std::endl;
        exit(1);
    }
}

void timer_handler(int sig)
{
    std::cout << "Caught the timer" << std::endl;
}

void setup_handler(){
    struct sigaction sa = {0};

    // Install timer_handler as the signal handler for SIGVTALRM.
    sa.sa_handler = &timer_handler;
    if (sigaction(SIGVTALRM, &sa, NULL) < 0)
    {
        std::cerr << SIGNAL_ERR << std:endl;
        exit(1);
    }
}


struct itimerval timer;
static Scheduler* scheduler;

int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        std::cerr << NON_POS << std::endl;
        return -1;
    }
    setup_timer(quantum_usecs, timer);
    setup_handler();
    scheduler = new Scheduler(quantum_usecs);
    return 0;
}


int uthread_spawn(thread_entry_point entry_point)
{
    return scheduler->spawn(entry_point);
}

int uthread_terminate(int tid)
{
    if (tid == 0)
    {
        delete scheduler;
        exit(0);
    }
    return scheduler->terminate_thread(tid);
}

int uthread_block(int tid)
{
    return scheduler->block(tid);
}

int uthread_resume(int tid)
{
    return scheduler->resume(tid);
}

int uthread_sleep(int num_quantums)
{
    return scheduler->sleep(num_quantums);
}

int uthread_get_tid()
{
    return scheduler->get_running_id();
}

int uthread_get_total_quantums()
{
    return scheduler->get_quantums();
}

int uthread_get_quantums(int tid)
{

    return scheduler->get_thread(tid).get_num_quantums();
}





int main()
{

    for (;;)
    {

    }
}

/*
 * Authors: Yitzchak Lindenbaum and Elay Aharoni
 * 14.04.2022
 */


//======================= includes ======================

#include <list>
#include <unordered_set>
#include <set>
#include <map>
#include <iostream>
#include <stdlib.h>
#include <signal.h>
#include <sys/time.h>
#include <errno.h>
#include "uthreads.h"
#include "thread.h"


//======================== macros =======================

#define SYS_ERR "system error: "
#define TIMER_ERR SYS_ERR << "failed to set timer"
#define SIGNAL_ERR SYS_ERR << "failed to set signal handler"

#define LIB_ERR "thread library error: "
#define DEAD_THREAD LIB_ERR << "thread does not exist"
#define MAIN_THREAD_BLOCK LIB_ERR << "main thread cannot be blocked"
#define NON_POS LIB_ERR << "quantum must be a positive integer"
#define NON_POS_NUMQ LIB_ERR << "Number of quantums must be a positive integer"
#define MAX_THREAD_EXCEEDED LIB_ERR << "Maximum number of threads exceeded"
#define NULL_ENTRY_POINT LIB_ERR << "entry point cannot be null"
#define MAIN_ID 0
#define ERR_EXIT_CODE
#define BLOCK_TIMER if (sigprocmask(SIG_BLOCK, &(handler).sa_mask, NULL) < 0) { \
                            std::cerr << SIGNAL_ERR << std::endl; \
                            uthread_terminate(MAIN_ID); \
                            }
#define UNBLOCK_TIMER if (sigprocmask(SIG_UNBLOCK, &(handler).sa_mask, NULL) < 0) { \
                            std::cerr << SIGNAL_ERR << std::endl; \
                            uthread_terminate(MAIN_ID); \
                            }

using namespace std;
typedef int tid_t;


//====================== Scheduler class ==================

void timer_handler(int sig);

/**
 * Class to represent scheduler object -- handles multi-thread programming using RR algorithm.
 */
class Scheduler
{
    int elapsed_quantums;
    list<tid_t> ready;
    set<tid_t> blocked;
    tid_t running;
    set<tid_t> available_ids;
    map<tid_t, int> sleeping;
    map<tid_t, Thread*> thread_map;
    struct itimerval timer;
    struct sigaction handler = {{0}};

private:
    /**
     * Indicates whether the given thread id belongs to a live thread
     * @param tid
     * @return true if alive, false otherwise
     */
    bool is_alive(tid_t tid)
    {return thread_map.count(tid);}

public:

    /**
     * Construct new Scheduler object (should be called once per program)
     * @param quantum_usecs Number of microseconds that should be considered a quantum. Must be positive.
     */
    Scheduler(int quantum_usecs)
            : elapsed_quantums(0), running(MAIN_ID)
    {
        // setup available id's set
        for (tid_t i = 0; i < MAX_THREAD_NUM; ++i)
        { available_ids.insert(i); }

        // setup main thread
        Thread* main_thread = new Thread(MAIN_ID, nullptr);
        available_ids.erase(available_ids.begin());
        thread_map.emplace(MAIN_ID, main_thread);
        ready.push_back(MAIN_ID); // necessary for technical reasons
        switch_to_next();

        // setup timer-handler and start timer
        setup_handler();
        setup_timer(quantum_usecs);
    }

    ~Scheduler() = default;

    /**
     * Create a new thread and place it in back of READY queue
     * @param entry_point pointer to function from which to start thread
     * @return 0 on success, -1 on failure
     */
    tid_t spawn(thread_entry_point entry_point)
    {
        BLOCK_TIMER;
        if(entry_point == nullptr){
            std::cerr << NULL_ENTRY_POINT << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        if (available_ids.empty())
        {
            UNBLOCK_TIMER;
            std::cerr << MAX_THREAD_EXCEEDED << std::endl;
            return -1;
        }

        tid_t next_id = *(available_ids.begin()); // ordered set keeps minimal value at beginning
        available_ids.erase(available_ids.begin());
        Thread* new_thread = new Thread(next_id, entry_point);
        thread_map.emplace(next_id, new_thread);
        ready.push_back(next_id);

        UNBLOCK_TIMER;

        return next_id;
    }

    /**
     * Terminates the thread with the given id
     * @param tid
     * @return 0 upon sucess, -1 on failure
     */
    int terminate_thread(tid_t tid)
    { // assumes that tid is positive
        BLOCK_TIMER;
        if (!is_alive(tid))
        {
            UNBLOCK_TIMER;
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }

        Thread *to_kill = thread_map.at(tid);
        thread_map.erase(tid);
        blocked.erase(tid);
        ready.remove(tid);
        delete to_kill;

        available_ids.insert(tid); // add id back into set so it can be reused

        if (running == tid) // thread is committing suicide
        {
            UNBLOCK_TIMER;
            start_new_quantum();
            switch_to_next();
        }

        UNBLOCK_TIMER;
        return 0;
    }


    /**
     * Switches to the next running thread in READY.
     * @param add_to_ready Should scheduler place the calling thread in the back end of READY?
     */
    void switch_to_next(bool add_to_ready=false)
    {
        if (ready.empty()) // the main thread is calling, there are no more ready threads. Func has no effect.
        {return;}

        BLOCK_TIMER;
        tid_t to_switch;
        do { // pop from READY until an unblocked one is found
            to_switch = ready.front();
            ready.pop_front();
            if (ready.empty()) {break;} // all threads but at most 1 in ready list were blocked
        } while(blocked.count(to_switch));
        if (blocked.count(to_switch)) {return;} // all threads were blocked - func has no effect

        if (add_to_ready) // condition entered only when switch happens because of timer
        {
            ready.push_back(running);
        }

        if (is_alive(running))
        { // save state of current thread before switching (unless switching because thread has committed suicide)
            int ret_val = sigsetjmp(thread_map.at(running)->get_env(), 1);
            if (ret_val)
            {
                UNBLOCK_TIMER;
                return;
            }
        }

        running = to_switch;
        UNBLOCK_TIMER;
        thread_map.at(running)->run();

    }


    /**
     * Block thread with given id
     * @param tid
     * @return 0 on success, -1 on failure
     */
    int block(tid_t tid)
    {
        BLOCK_TIMER;
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        if (tid == MAIN_ID)
        {
            std::cerr << MAIN_THREAD_BLOCK << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        blocked.insert(tid);
        if (tid == running) // thread blocked itself
        {
            UNBLOCK_TIMER;
            start_new_quantum();
            switch_to_next();
        }

        UNBLOCK_TIMER;
        return 0;
    }


    /**
     * Unblock thread with given id
     * @param tid
     * @return 0 on sucess, -1 on failure
     */
    int resume(tid_t tid)
    {
        BLOCK_TIMER;
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }
        // if not in blocked, func should have no effect
        if (blocked.count(tid))
        {
            blocked.erase(tid);
            if (!sleeping.count(tid)) {
                ready.push_back(tid);
            }
        }

        UNBLOCK_TIMER;
        return 0;
    }


    /**
     * Put running thread to sleep for given number of quantums
     * @param num_quantums number of quantums to sleep for
     * @return 0 on success, -1 on failure
     */
    int sleep(int num_quantums)
    {
        BLOCK_TIMER;
        if (num_quantums <= 0) {
            std::cerr << NON_POS_NUMQ << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        if (running == MAIN_ID) {
            std::cerr << MAIN_THREAD_BLOCK << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        sleeping.emplace(running, num_quantums + 1); // add 1 because the first quantum transition doesn't count

        UNBLOCK_TIMER;
        start_new_quantum();
        switch_to_next();
        return 0;
    }


    /**
     * Iterates over sleeping threads and updates them. Wakes them if it is time. To be called every time
     * a new quantum begins.
     */
    void update_and_wake() { // must be called by timer handler
        for (auto it = sleeping.begin(); it != sleeping.end();) {
            tid_t tid = it->first;

            if (--(sleeping.at(tid)) == 0) {
                sleeping.erase(it++);
                if (is_alive(tid) && !(blocked.count(tid))) { // put awoken thread in READY iff it is alive and
                    ready.push_back(tid);                      // not blocked
                }
            }
            else {
                ++it;
            }
        }
    }


    /**
     * @return id of currently running thread
     */
    tid_t get_running_id() const
    {return running;}


    /**
     * @return total number of elapsed quantums
     */
    int get_quantums() const
    {return elapsed_quantums;}


    /**
     * Get pointer of thread with given id
     * @param tid
     * @return pointer to thread, null if does not exist
     */
    Thread* get_thread(tid_t tid)
    {
      if (!is_alive(tid))
      {
        std::cerr << DEAD_THREAD << std::endl;
        return nullptr;
      }
        return thread_map.at(tid);
    }


    /**
     * Initialize timer
     * @param quantum_usecs amount of time to set timer to, in microseconds
     */
    void setup_timer(int quantum_usecs) {
        timer.it_value.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_value.tv_usec = quantum_usecs % 1000000;
        timer.it_interval.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_interval.tv_usec = quantum_usecs % 1000000;
        start_new_quantum();
    }


    /**
     * Begin a new quantum. To be called when timer must be manually (re)set.
     */
    void start_new_quantum(){
        if (setitimer(ITIMER_VIRTUAL, &timer, nullptr)) {
            std::cerr << TIMER_ERR << std::endl;
            uthread_terminate(MAIN_ID);
        }

        elapsed_quantums++;
        update_and_wake();
    }


    /**
     * Initialize timer-signal handler
     */
    void setup_handler(){

        handler.sa_handler = &timer_handler;
        if (sigaction(SIGVTALRM, &handler, nullptr) < 0)
        {
            std::cerr << SIGNAL_ERR << std::endl;
            uthread_terminate(MAIN_ID);
        }
        sigemptyset(&(handler).sa_mask);
        sigaddset(&(handler).sa_mask, SIGVTALRM);
    }


    /**
     * Increase field tracking number of elapsed quantums by 1.
     */
    void elapse_quantum(){elapsed_quantums++;}
};


//====================== uthread library ====================


Scheduler* scheduler;

/**
 * Function to be set as handler for timer signal
 * @param sig
 */
void timer_handler(int sig)
{
    scheduler->elapse_quantum();
    scheduler->update_and_wake();
    scheduler->switch_to_next(true);
}

/// See documentation for uthreads library in provided uthreads.h file
int uthread_init(int quantum_usecs)
{
    if (quantum_usecs <= 0)
    {
        std::cerr << NON_POS << std::endl;
        return -1;
    }
    scheduler = new Scheduler(quantum_usecs);
    return 0;
}


int uthread_spawn(thread_entry_point entry_point)
{return scheduler->spawn(entry_point);}


int uthread_terminate(int tid)
{
    if (tid == 0)
    {
        delete scheduler;
        if (!errno) {exit(0);}
        else {exit(1);}
    }
    return scheduler->terminate_thread(tid);
}


int uthread_block(int tid)
{return scheduler->block(tid);}


int uthread_resume(int tid)
{return scheduler->resume(tid);}


int uthread_sleep(int num_quantums)
{return scheduler->sleep(num_quantums);}


int uthread_get_tid()
{return scheduler->get_running_id();}


int uthread_get_total_quantums()
{return scheduler->get_quantums();}


int uthread_get_quantums(int tid)
{return scheduler->get_thread(tid)->get_num_quantums();}
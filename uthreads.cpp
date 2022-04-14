
//======================= includes ======================

#include <list>
#include <unordered_set>
#include <set>
#include <map>
#include <iostream>
#include "stdlib.h"
#include <signal.h>
#include <sys/time.h>
#include "uthreads.h"
#include "thread.h"

//sigpromask(SIG_BLOCK, &(handler).sa_mask, NULL);
//sigpromask(SIG_UNBLOCK, &(handler).sa_mask, NULL);
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


using namespace std;
typedef unsigned int id_t;


//====================== the scheduler ==================

void timer_handler(int sig);

class Scheduler
{
    int quantum_usecs;
    int elapsed_quantums;
    list<id_t> ready;
    set<id_t> blocked;
    id_t running;
    set<id_t> available_ids;
    map<id_t, int> sleeping;
    map<id_t, Thread*> thread_map;
    struct itimerval timer;
    struct sigaction handler = {0};

public:


    Scheduler(int quantum_usecs)
            : quantum_usecs(quantum_usecs), running(0), elapsed_quantums(0)
    {
        for (id_t i = 0; i < MAX_THREAD_NUM; ++i)
        { available_ids.insert(i); }
        Thread *main_thread = new Thread(*(available_ids.begin()), nullptr);
        available_ids.erase(available_ids.begin());
        thread_map.emplace(0, main_thread);
        ready.push_back(0);
        switch_to_next();
        setup_timer();
        setup_handler();
    }


    ~Scheduler() = default;
    /*
    ~Scheduler() {
        delete &thread_map.at(0);
    };
    */


    id_t spawn(thread_entry_point entry_point)
    {

        if(entry_point == nullptr){
            std::cerr << NULL_ENTRY_POINT << std::endl;
            return -1;
        }

        //sigpromask(SIG_BLOCK, &(handler).sa_mask, NULL);

        if (available_ids.empty())
        {
            std::cerr << MAX_THREAD_EXCEEDED << std::endl;
            return -1;
        }

        id_t next_id = *(available_ids.begin());
        available_ids.erase(available_ids.begin());
        Thread *new_thread = new Thread(next_id, entry_point);
        ready.push_back(next_id);
        thread_map.emplace(next_id, new_thread);
        return next_id;
    }


    bool is_alive(id_t tid)
    {return thread_map.count(tid);}


    int terminate_thread(id_t tid)
    { // assumes that tid is positive
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            return -1;
        }

        Thread *to_kill = thread_map.at(tid);
        thread_map.erase(tid);
        blocked.erase(tid);
        ready.remove(tid); // this may not be necessary since we check if the next ready thread is alive anyway, and
        // searching a linked list is inefficient
        delete to_kill;

        if (running == tid || tid == 0)
        {
            reset_timer();
            switch_to_next();
           /* running = ready.front();
            ready.pop_front();
            thread_map.at(running)->set_state(RUNNING);
            thread_map.at(running)->run();*/
        }
        return 0;
    }


    void switch_to_next(bool add_to_ready=false)
    {
        if (ready.empty())
        { return; } // main thread is running and ready is empty - continue running

        //sigpromask(SIG_BLOCK, &(handler).sa_mask, NULL);
        id_t to_switch;
        do { // pop from READY until a live thread is found
            to_switch = ready.front();
            ready.pop_front();
        } while(!is_alive(to_switch));


        if (add_to_ready) // place running thread in READY iff it has not just blocked itself
        {
            ready.push_back(running);
            thread_map.at(running)->set_state(READY);
        }

        if (is_alive(running))
        { // save state of current thread before switching
            int ret_val = sigsetjmp(thread_map.at(running)->get_env(), 1);
            if (ret_val)
            { return; }
        }

        running = to_switch;
        thread_map.at(running)->set_state(RUNNING);
        //sigpromask(SIG_UNBLOCK, &(handler).sa_mask, NULL);
        thread_map.at(running)->run();

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
        thread_map.at(running)->set_state(BLOCKED);
        if (tid == running)
        {
            reset_timer();
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
                thread_map.at(tid)->set_state(READY);
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

        sleeping.emplace(running, num_quantums + 1);
        thread_map.at(running)->set_state(SLEEPING);
        reset_timer();
        switch_to_next();
        return 0;
    }


    void update_and_wake() { // must be called by timer handler
        for (auto it = sleeping.begin(); it != sleeping.end();) {
            id_t tid = it->first;
            if (--(sleeping.at(tid)) == 0) {
                sleeping.erase(it++);
                if (is_alive(tid) && !(blocked.count(tid))) {
                    ready.push_back(tid);
                    thread_map.at(tid)->set_state(READY);
                }
            }
            else {
                ++it;
            }
        }
    }


    id_t get_running_id() const
    {return running;}


    int get_quantums() const
    {return elapsed_quantums;}


    Thread* get_thread(id_t tid)
    {
      if (!is_alive(tid))
      {
        std::cerr << DEAD_THREAD << std::endl;
        return nullptr;
      }
        return thread_map.at(tid);
    }


    void setup_timer() {
        timer.it_value.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_value.tv_usec = quantum_usecs % 1000000;
        timer.it_interval.tv_sec = quantum_usecs / (int) 1000000;
        timer.it_interval.tv_usec = quantum_usecs % 1000000;
        reset_timer();
    }


    void reset_timer(){
        if (setitimer(ITIMER_VIRTUAL, &timer, nullptr)) {
            std::cerr << TIMER_ERR << std::endl;
            exit(1);
        }
        elapsed_quantums++;
    }


    void setup_handler(){

        handler.sa_handler = &timer_handler;
        if (sigaction(SIGVTALRM, &handler, nullptr) < 0)
        {
            std::cerr << SIGNAL_ERR << std::endl;
            exit(1);
        }
        sigemptyset(&(handler).sa_mask);
        sigaddset(&(handler).sa_mask, SIGVTALRM);
    }


    void elapse_quantum(){elapsed_quantums++;}
};





//====================== uthread library ====================


static Scheduler* scheduler;


void timer_handler(int sig)
{
    //std::cout << scheduler->get_quantums() << std::endl;
    //std::cout << "running : " << scheduler->get_running_id() << std::endl;
    scheduler->elapse_quantum();
    scheduler->update_and_wake();
    scheduler->switch_to_next(true);

}

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
        exit(0);
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

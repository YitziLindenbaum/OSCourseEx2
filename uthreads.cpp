
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
#define BLOCK_TIMER sigprocmask(SIG_BLOCK, &(handler).sa_mask, NULL)
#define UNBLOCK_TIMER sigprocmask(SIG_UNBLOCK, &(handler).sa_mask, NULL)

using namespace std;
typedef int tid_t;


//====================== the scheduler ==================

void timer_handler(int sig);

class Scheduler
{
    int quantum_usecs;
    int elapsed_quantums;
    list<tid_t> ready;
    set<tid_t> blocked;
    tid_t running;
    set<tid_t> available_ids;
    map<tid_t, int> sleeping;
    map<tid_t, Thread*> thread_map;
    struct itimerval timer;
    struct sigaction handler = {0};

public:


    Scheduler(int quantum_usecs)
            : quantum_usecs(quantum_usecs), running(0), elapsed_quantums(0)
    {
        for (tid_t i = 0; i < MAX_THREAD_NUM; ++i)
        { available_ids.insert(i); }
        Thread* main_thread = new Thread(*(available_ids.begin()), nullptr);
        available_ids.erase(available_ids.begin());
        thread_map.emplace(0, main_thread);
        ready.push_back(0);
        switch_to_next();
        setup_handler();
        setup_timer();
    }


    ~Scheduler() = default;

    /*~Scheduler() {
        delete &thread_map.at(0);
    };*/



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
            std::cerr << MAX_THREAD_EXCEEDED << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        tid_t next_id = *(available_ids.begin());
        available_ids.erase(available_ids.begin());
        Thread* new_thread = new Thread(next_id, entry_point);
        thread_map.emplace(next_id, new_thread);
        ready.push_back(next_id);
        UNBLOCK_TIMER;
        return next_id;
    }


    bool is_alive(tid_t tid)
    {return thread_map.count(tid);}


    int terminate_thread(tid_t tid)
    { // assumes that tid is positive
        BLOCK_TIMER;
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            UNBLOCK_TIMER;
            return -1;
        }

        Thread *to_kill = thread_map.at(tid);
        thread_map.erase(tid);
        blocked.erase(tid);
        ready.remove(tid);
        delete to_kill;
        available_ids.insert(tid);

        if (running == tid)
        {
            UNBLOCK_TIMER;
            reset_timer();
            switch_to_next();
        }

        //std::cout << tid << " was killed in the " << elapsed_quantums << " quantum" << endl;

        UNBLOCK_TIMER;
        return 0;
    }


    void switch_to_next(bool add_to_ready=false)
    {
        if (ready.empty())
        {return;}

        BLOCK_TIMER;
        tid_t to_switch;
        do { // #todo: check,,,,,, pop from READY until a live thread is found
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
            {
                UNBLOCK_TIMER;
                return;
            }
        }

        //std::cout << "swiched between " << running << " to " << to_switch << " in the " << elapsed_quantums << " quantum" << endl;

        running = to_switch;
        thread_map.at(running)->set_state(RUNNING);
        UNBLOCK_TIMER;
        thread_map.at(running)->run();

    }


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
        ready.remove(tid);
        blocked.insert(tid);
        thread_map.at(running)->set_state(BLOCKED);
        if (tid == running)
        {
            UNBLOCK_TIMER;
            reset_timer();
            switch_to_next();
        }

        std::cout <<"@@@" << tid << " blocked in the " << elapsed_quantums << " quantum" << endl;

        UNBLOCK_TIMER;
        return 0;
    }


    int resume(tid_t tid)
    {
        BLOCK_TIMER;
        if (!is_alive(tid))
        {
            std::cerr << DEAD_THREAD << std::endl;
            UNBLOCK_TIMER;
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

        std::cout <<"@@@" << tid << " resumed in the " << elapsed_quantums << " quantum" << endl;

        UNBLOCK_TIMER;
        return 0;
    }


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

        sleeping.emplace(running, num_quantums + 1);
        thread_map.at(running)->set_state(SLEEPING);

        std::cout <<"@@@" << running << "  went to sleep for: " << num_quantums << " in the " << elapsed_quantums << " quantum" << endl;

        UNBLOCK_TIMER;
        reset_timer();
        switch_to_next();
        return 0;
    }


    void update_and_wake() { // must be called by timer handler
        for (auto it = sleeping.begin(); it != sleeping.end();) {
            tid_t tid = it->first;

            std::cout <<"@@@" << tid << " has " << sleeping.at(tid) << " quantums left to sleep " << "currently " <<  elapsed_quantums << " quantum" << endl;

            if (--(sleeping.at(tid)) == 0) {
                sleeping.erase(it++);

                std::cout <<"@@@" << tid << "  woke up in the " << elapsed_quantums << " quantum" << endl;

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


    tid_t get_running_id() const
    {return running;}


    int get_quantums() const
    {return elapsed_quantums;}


    Thread* get_thread(tid_t tid)
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

        //std::cout << "reset timer" << endl;

        elapsed_quantums++;
        update_and_wake();
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


Scheduler* scheduler;


void timer_handler(int sig)
{

    //std::cout << "caught the timer" << endl;

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
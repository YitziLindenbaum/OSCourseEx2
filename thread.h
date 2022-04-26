#ifndef EX2_THREAD_H
#define EX2_THREAD_H

//================ includes =================

#include <setjmp.h>

//================ macros ===================

typedef int tid_t;
typedef void (*thread_entry_point)(void);

//================== thread declaration ===========
/**
 * Class represents a thread in multi-thread programming.
 */
class Thread {
private:
    tid_t id;
    thread_entry_point initial_entry_point;
    char* initial_sp;
    int num_running_quantums;
    jmp_buf env;


public:
    Thread(unsigned int id, thread_entry_point entry_point);
    ~Thread();
    void run();
    tid_t get_id() const;
    thread_entry_point get_init_entry_point() const;
    jmp_buf &get_env();
    int get_num_quantums() const;
};

#endif //EX2_THREAD_H

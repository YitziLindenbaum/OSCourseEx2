#ifndef EX2_THREAD_H
#define EX2_THREAD_H

//================ includes =================

#include <setjmp.h>

//================ macros ===================

enum State {READY, RUNNING, BLOCKED, SLEEPING};
typedef unsigned int id_t;
typedef void (*thread_entry_point)(void);

//================== thread declaration ===========
class Thread {
private:
    id_t id;
    thread_entry_point initial_entry_point;
    char* initial_sp;
    State state;
    int num_running_quantums;
    jmp_buf env;


public:
    Thread(unsigned int id, thread_entry_point entry_point);
    ~Thread();
    void run();
    void set_state(State new_state);
    id_t get_id() const;
    thread_entry_point get_init_entry_point() const;
    State get_state() const;
    jmp_buf &get_env();
    int get_num_quantums() const;
};

#endif //EX2_THREAD_H

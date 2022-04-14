//============== includes ================

#include "uthreads.h"
#include "thread.h"
#include <signal.h>


//==================== macros ================

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7


//============== black box function =============

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

//================= thread implementation ==============

Thread::Thread(const unsigned int id, const thread_entry_point entry_point)
    : id(id), initial_entry_point(entry_point)
    {
    char* stack = new char[STACK_SIZE];
    initial_sp = stack;
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    auto pc = (address_t) entry_point;
    sigsetjmp(env, 1);
    (env->__jmpbuf)[JB_SP] = translate_address(sp);
    (env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&env->__saved_mask);
    num_running_quantums = 0;
    }


Thread::~Thread()
{ delete[] initial_sp;}


void Thread::run()
{
    num_running_quantums ++;
    siglongjmp(env, 1);
}


void Thread::set_state(State new_state)
{this->state = new_state;}


tid_t Thread::get_id() const
{return this->id;}


int Thread::get_num_quantums() const
{return num_running_quantums;}


thread_entry_point Thread::get_init_entry_point() const
{return this->initial_entry_point;}


State Thread::get_state() const
{return this->state;}


jmp_buf &Thread::get_env()
{return this->env;}


//
// Created by chucklind on 04/04/2022.
//

#include "uthreads.h"
#include "thread.h"

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%fs:0x30,%0\n"
                 "rol    $0x11,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

Thread::Thread(const unsigned int id, const thread_entry_point entry_point)
    : id(id), initial_entry_point(entry_point)
    {
    char* stack = new char[STACK_SIZE];
    initial_sp = stack;
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entry_point;
    sigsetjmp(env, 1);
    env[JB_SP] = translate_address(sp);
    env[JB_PC] = translate_address(pc);
    }

Thread::~Thread(){ delete[] initial_sp; }

void Thread::run(){
    siglongjmp(env, 1);
}

int Thread::die(){

}

void Thread::set_state(State state){
    this->state = state;
    }


id_t Thread::get_id() const{
    return this->id;
    }



thread_entry_point Thread::get_init_entry_point() const{
    return this->initial_entry_point;
    }

State Thread::get_state() const{
    return this->state;
    }

jmp_buf &Thread::get_env()
{
    return this->env;
}


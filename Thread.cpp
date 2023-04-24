#include "Thread.h"

// This code was yoinked from demo_jmp.c

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
               : "=g"(ret)
               : "0"(addr));
  return ret;
}

#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr) {
  address_t ret;
  asm volatile("xor    %%gs:0x18,%0\n"
               "rol    $0x9,%0\n"
               : "=g"(ret)
               : "0"(addr));
  return ret;
}

#endif

int Thread::save_env(int val) {
  return sigsetjmp(_env, val); // saves the current environment in the buffer
}

void Thread::restore_env(int val) {
  siglongjmp(_env, val); // restores the environment from the buffer
}

Thread::Thread(State state, int tid, thread_entry_point f)
    : _state(state), _tid(tid), _entry_point(f) {

  // this sets the stack pointer and the program counter to the values we want

  address_t sp, pc;
  sp = (address_t)_stack + STACK_SIZE - sizeof(address_t);
  pc = (address_t)_entry_point;
  save_env();
  (_env->__jmpbuf)[JB_SP] = translate_address(sp);
  (_env->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&_env->__saved_mask); //! floating return value if failed
}

// Getters and setters:

int Thread::tid() const { return _tid; }

int Thread::sleeping_time() const { return _sleeping_time; }

int Thread::quantum() const { return running_quantum; }

State Thread::state() const { return _state; }

bool Thread::explicity_blocked() const { return _explicity_blocked; }

thread_entry_point Thread::entry_point() const { return _entry_point; }

sigjmp_buf &Thread::env() { return _env; }

void Thread::set_state(State state) { _state = state; }

void Thread::set_tid(int tid) { _tid = tid; }

void Thread::set_entry_point(thread_entry_point f) { _entry_point = f; }

void Thread::set_sleeping(int val) { _sleeping_time = val; }

void Thread::inc_quantum() { running_quantum++; }

void Thread::dec_sleeping_time() { _sleeping_time--; }

void Thread::set_explicity_blocked(bool val) { _explicity_blocked = val; }

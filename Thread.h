#ifndef THREAD_H
#define THREAD_H

#include "uthreads.h"
#include <algorithm>
#include <deque>
#include <iostream>
#include <map>
#include <math.h>
#include <setjmp.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string>
#include <sys/time.h>
#include <unistd.h>
#include <vector>

#define STACK_SIZE 4096 /* stack size per thread (in bytes) */
typedef void (*thread_entry_point)(void);

enum State { READY, RUNNING, BLOCKED };

class Thread {

private:
  State _state;
  int _tid;
  thread_entry_point _entry_point;
  sigjmp_buf _env;
  char _stack[STACK_SIZE];
  int _sleeping_time = 0;
  int running_quantum = 0;
  bool _explicity_blocked = false;

public:
  Thread(State state, int tid, thread_entry_point f);

  // Getters:

  // notice they're named the same as the private members (for ease of use)

  State state() const;
  int tid() const;
  int sleeping_time() const;
  thread_entry_point entry_point() const;
  bool explicity_blocked() const;
  int quantum() const;
  sigjmp_buf &
  env(); // allows for the buffer to be changed (none-const reference)

  // Setters:

  void set_state(State state);
  void set_tid(int tid);
  void set_entry_point(thread_entry_point f);
  void set_sleeping(int val);
  void inc_quantum();
  void dec_sleeping_time();
  void set_explicity_blocked(bool val);

  /**
    Since we will handle the buffers in the Thread class, we will need to
    Save the environment of the current thread, and restore the environment
  */

  int save_env(int val = 1);
  void restore_env(int val = 1);
};

#endif // THREAD_H
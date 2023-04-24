
#include "uthreads.h"
#include "Thread.h"
#include <memory>

#define MICROSECS_IN_SEC(x) (floor(x / 1000000))
#define BLK_SIG -5
#define check_error(condition, message)                                       \
  if ((condition == -1)) {                                                    \
    std::cerr << message << std::endl;                                        \
    return -1;                                                                \
  }

typedef std::shared_ptr<Thread> ThreadPtr;

void signal_handler(int sig);
void schedule();
int uthread_get_quantums(int tid);
int uthread_get_total_quantums();
int update_sleeping_threads();

// * Scheduler  System * //
std::deque<ThreadPtr> ready_queue; // a queue of all the threads that are READY
std::map<int, ThreadPtr> threads_map;  // maps tid to thread
std::map<int, ThreadPtr> blocked_map;  // maps tid to thread
std::map<int, ThreadPtr> sleeping_map; // maps tid to thread
ThreadPtr cur_running_thread;          // the current running thread
int to_be_deleted_thread = -1; // if not -1, delete the thread with this tid
int default_quantum_usecs;
int total_quantums = 0; // the total number of quantums that have passed

struct sigaction sa = {0};

struct itimerval timer;

int add_signals() {
  int ret_value = sigemptyset(&(sa.sa_mask));
  check_error(ret_value, "system error: sigemptyset failed");
  ret_value = sigaddset(&(sa.sa_mask), SIGVTALRM);
  check_error(ret_value, "system error: sigaddset failed");
  return 0;
}

int block_signal() {
  int ret_value = sigprocmask(SIG_BLOCK, &(sa.sa_mask), NULL);
  check_error(ret_value, "system error: sigprocmask failed");
  return 0;
}

int unblock_signal() {
  int ret_value = sigprocmask(SIG_UNBLOCK, &(sa.sa_mask), NULL);
  check_error(ret_value, "system error: sigprocmask failed");
  return 0;
}

void insert_thread(ThreadPtr thread_ptr) {
  thread_ptr->set_state(READY);
  ready_queue.push_back(thread_ptr);
  threads_map[thread_ptr->tid()] = thread_ptr;
}

void insert_first_thread(ThreadPtr thread_ptr) {
  cur_running_thread = thread_ptr;
  threads_map[thread_ptr->tid()] = thread_ptr;
}

int init_scheduler() {
  sa.sa_handler = &signal_handler;
  int ret = add_signals();
  check_error(ret, "system error: add_signals failed");
  ret = sigaction(SIGVTALRM, &sa, NULL);
  return ret;
}

void jump_to_thread(int tid) {
  cur_running_thread = threads_map[tid];
  cur_running_thread->restore_env();
}

int set_timer() {
  timer.it_value.tv_sec = MICROSECS_IN_SEC(default_quantum_usecs);
  timer.it_value.tv_usec = default_quantum_usecs % 1000000;
  timer.it_interval.tv_sec = MICROSECS_IN_SEC(default_quantum_usecs);
  timer.it_interval.tv_usec = default_quantum_usecs % 1000000;

  int ret = setitimer(ITIMER_VIRTUAL, &timer, NULL);

  return ret;
}

void remove_from_ready_queue(int tid) {
  for (auto it = ready_queue.begin(); it != ready_queue.end(); ++it) {
    if ((*it)->tid() == tid) {
      ready_queue.erase(it);
      break;
    }
  }
}

void remove_thread_from_scheduler(int tid) {
  // if it was the current running thread:
  if (cur_running_thread->tid() == tid) {
    if (ready_queue.empty()) {
      cur_running_thread = nullptr;

    } else {
      to_be_deleted_thread = tid;
      signal_handler(0);
    }
  } else {
    remove_from_ready_queue(tid);
  }
}

void preempt() {
  cur_running_thread->set_state(READY);
  ready_queue.push_back(cur_running_thread);
}

void schedule() {

  cur_running_thread = ready_queue.front();
  ready_queue.pop_front();
  cur_running_thread->set_state(RUNNING);
  cur_running_thread->inc_quantum();
  total_quantums++;
}

void yield(int sig) {
  int ret_val = 0;
  if (ready_queue.empty()) { // sanity check
    update_sleeping_threads();
    set_timer();
    return;
  }
  if (sig != BLK_SIG) {
    ret_val = ready_queue.back()->save_env(); // previous thread
  } else {
    threads_map[to_be_deleted_thread]->set_state(BLOCKED);
    ret_val = threads_map[to_be_deleted_thread]->save_env();
  }
  if (to_be_deleted_thread != -1) {
    remove_from_ready_queue(to_be_deleted_thread);
    to_be_deleted_thread = -1;
  }

  update_sleeping_threads();
  bool did_just_save_bookmark = ret_val == 0;

  if (did_just_save_bookmark) {
    jump_to_thread(cur_running_thread->tid());
  } else {
    set_timer();
  }
}

void signal_handler(int sig) {

  // case BLKSIG:
  block_signal();
  preempt();
  schedule();
  yield(sig);
  unblock_signal();
}

// * Id system * //
typedef std::vector<int> id_vector;
id_vector possible_ids; // when a thread is terminated, its id is pushed back
                        // to this vector
void init_ids() {
  for (int i = 1; i < MAX_THREAD_NUM; i++) {
    possible_ids.push_back(i);
  }
}

int get_next_id() {
  int id = possible_ids.front();
  possible_ids.erase(possible_ids.begin());
  return id;
}

int add_id(int id) {
  possible_ids.insert(possible_ids.begin(), id);
  std::sort(possible_ids.begin(), possible_ids.end());
  return 0;
}

// library functions:

int uthread_init(int quantum_usecs) {

  int tid = 0;

  if (quantum_usecs <= 0) {
    check_error(-1, "thread library error: quantum_usecs must be positive");
  }
  default_quantum_usecs = quantum_usecs;
  init_ids();
  ThreadPtr main_thread(new Thread(RUNNING, tid, nullptr));
  insert_first_thread(main_thread);

  init_scheduler();

  main_thread->inc_quantum();
  total_quantums++;

  check_error(set_timer(), "system error: set_timer failed");

  return 0;
}

int uthread_spawn(thread_entry_point entry_point) {
  block_signal();
  if (possible_ids.empty()) {
    unblock_signal();
    check_error(-1, "thread library error: no more threads can be created");
  }
  if (entry_point == nullptr) {
    unblock_signal();
    check_error(-1, "thread library error: entry_point is null");
  }

  int tid = get_next_id();
  ThreadPtr thread(new Thread(READY, tid, entry_point));
  insert_thread(thread);
  // check_error(block_sigvtalrm(thread), "");

  unblock_signal();
  return tid;
}

int uthread_terminate(int tid) {
  block_signal();
  if (threads_map.find(tid) == threads_map.end()) {
    unblock_signal();
    check_error(-1, "thread library error: thread does not exist");
  }

  if (threads_map[tid] == nullptr) {
    unblock_signal();
    check_error(-1, "thread library error: thread does not exist");
  }
  if (tid == 0) {
    // delete everything

    exit(0);
  } else {

    switch (threads_map[tid]->state()) {
    case READY:
      remove_thread_from_scheduler(tid);
      break;
    case RUNNING:

      threads_map.erase(tid);
      add_id(tid);
      remove_thread_from_scheduler(tid);
      break;
    case BLOCKED:
      blocked_map.erase(tid);
      auto it = sleeping_map.find(tid);
      if (it != sleeping_map.end()) {
        sleeping_map.erase(it);
      }
      break;
    }
    threads_map.erase(tid);
    add_id(tid);
    unblock_signal();
    return 0;
  }
}

int uthread_block(int tid) {

  block_signal();
  if (tid == 0) {
    unblock_signal();
    check_error(-1, "thread library error: cannot block main thread");
  }
  if (threads_map.find(tid) == threads_map.end()) {
    unblock_signal();
    check_error(-1, "thread library error: thread does not exist");
  }
  switch (threads_map[tid]->state()) {
  case READY:
    remove_from_ready_queue(tid);
    threads_map[tid]->set_state(BLOCKED);
    threads_map[tid]->set_explicity_blocked(true);
    blocked_map[tid] = threads_map[tid];
    break;
  case RUNNING:
    blocked_map[tid] = cur_running_thread;
    threads_map[tid]->set_explicity_blocked(true);
    to_be_deleted_thread = tid;
    signal_handler(BLK_SIG);
    break;
  case BLOCKED:
    threads_map[tid]->set_explicity_blocked(true);
    break;
  }
  unblock_signal();
  return 0;
}

int uthread_resume(int tid) {
  block_signal();
  if (threads_map.find(tid) == threads_map.end()) {
    unblock_signal();
    check_error(-1, "thread library error: thread does not exist");
  }
  if (threads_map[tid]->sleeping_time() > 0) {
    threads_map[tid]->set_explicity_blocked(false);
  } else if (threads_map[tid]->state() == BLOCKED) {
    threads_map[tid]->set_explicity_blocked(false);
    insert_thread(threads_map[tid]);
  }
  auto it = blocked_map.find(tid);
  if (it != blocked_map.end()) {
    blocked_map.erase(it);
  }
  unblock_signal();
  return 0;
}

int update_sleeping_threads() {
  for (auto it = sleeping_map.begin(); it != sleeping_map.end();) {

    if (it->second->sleeping_time() > 1) {
      it->second->dec_sleeping_time();
      ++it;

    } else if (it->second->sleeping_time() == 1) {
      if (!(it->second->explicity_blocked())) {

        it->second->set_state(READY);

        ready_queue.push_back(it->second);
      }
      it->second->dec_sleeping_time();

      it = sleeping_map.erase(it);

    } else {

      exit(1);
    }
  }
  return 0;
}

int uthread_sleep(int num_quantums) {
  if (num_quantums <= 0) {
    check_error(-1, "thread library error: num_quantums must be positive");
  }
  if (cur_running_thread->tid() == 0) {
    check_error(-1, "thread library error: cannot block main thread");
  }
  cur_running_thread->set_sleeping(num_quantums);
  sleeping_map[cur_running_thread->tid()] = cur_running_thread;
  to_be_deleted_thread = cur_running_thread->tid();
  signal_handler(BLK_SIG);
  return 0;
}

int uthread_get_tid() { return cur_running_thread->tid(); }

int uthread_get_total_quantums() { return total_quantums; }

int uthread_get_quantums(int tid) {
  auto it = threads_map.find(tid);

  if (it == threads_map.end()) {

    check_error(-1, "thread library error: thread doesn't exist");
  }

  return threads_map[tid]->quantum();
}
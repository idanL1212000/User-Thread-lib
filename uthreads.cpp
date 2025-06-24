
#include <stdio.h>
#include <stdlib.h> 
#include <signal.h> 
#include <sys/time.h> 
#include <setjmp.h>
#include "uthreads.h"
#include "demo_jmp.c"
#include <queue>
#include <map>

#define LIB_ERROR -1
#define QUANTUM_ERROR_MASSAGE "thread library error: needa positive num\
  for quantum_usecs\n"
#define SPAWN_ERROR_MASSAGE "thread library error: to many threads\n"
#define SPAWN_ENTRY_ERROR_MASSAGE "thread library error: entry pointer is null\n"
#define INVALID_THREAD_ID_ERROR_MASSAGE "thread library error: invalid id \n"
#define BLOCKED_MAIN_THREAD_ERROR_MASSAGE "thread library error: can't block\
                                            or put to sleep the main thread\n"
#define THREAD_NOT_FOUND_ERROR_MASSAGE "\n"
#define SIG_JUMP_ERROR_MESSAGE "system error:failed to context switch threads\n"
#define SET_TIME_ERROR_MESSAGE "system error: setitimer error\n"
#define SIG_ACK_ERROR_MESSAGE "system error: sigaction error\n"
#define INTERRUPT_ERROR_MESSAGE "system error: interrupt error\n"
#define MALLOC_ERROR_MASSAGE "system error: malloc error\n"

// Define the states for the threads
enum ThreadState {
    RUNNING,
    READY,
    BLOCKED,
    TERMINATED
};

// Structure to represent a thread
struct Thread {
    int tid; // Thread ID
    enum ThreadState state; // Thread state
    char *stack; // Pointer to the thread's stack
    thread_entry_point entry_point; // Entry point function pointer
    int quantum_count = 0; // Number of quantums the thread has run
    int sleeping = 0;
};


// Function to get the ID of the next available thread
int get_next_tid();
bool is_ready_queue_empty();
Thread *get_thread (int tid);
void end_lib();
Thread *get_next_thread ();
void free_thread(Thread* thread);

int uthread_init(int quantum_usecs);
int uthread_spawn(thread_entry_point entry_point);
int uthread_terminate(int tid);
int uthread_block(int tid);
int uthread_resume(int tid);
int uthread_sleep(int num_quantums);
int uthread_get_tid();
int uthread_get_total_quantums();
int uthread_get_quantums(int tid);

// Global variables
// Set up the timer interrupt handler
struct sigaction scheduler = {0};
struct itimerval timer;
int global_quantum_usecs; // Length of a quantum in microseconds
int total_quantums = 0; // Total number of quantums since library initialization
bool ending_lib = false;

std::priority_queue<int, std::vector<int>, std::greater<int>> id_queue;

Thread* global_current_thread;// Current thread
// Queue to store READY threads
std::queue<Thread*> ready_queue;
std::queue<Thread*> remove_queue;
std::map<int,Thread*> blocked_map;
std::queue<Thread*> sleep_queue;
sigjmp_buf env[MAX_THREAD_NUM];

void lib_push(Thread* thread){
  if(thread->sleeping > 0){
    sleep_queue.push (thread);
  }
  switch (thread->state)
  {
    case READY:
      ready_queue.push (thread);
      break;
    case BLOCKED:
      blocked_map[thread->tid] = thread;
      break;
    case TERMINATED:
      remove_queue.push (thread);
      break;
    case RUNNING:
      break;
  }
}

void clean_deleted_thread(){
  while (!remove_queue.empty()){
    Thread* thread = remove_queue.front();
    remove_queue.pop();
    free_thread (thread);
  }
}

void update_sleep_queue(){
  std::queue<Thread*> temp_queue;
  while (!sleep_queue.empty()){
    Thread* thread = sleep_queue.front();
    sleep_queue.pop();
    thread->sleeping -= 1;
    if(thread->sleeping == 0){
      if(thread->state == READY){ready_queue.push (thread);}
      if(thread->state == BLOCKED){blocked_map[thread->tid] = thread;}
    }
    if(thread->state != TERMINATED){temp_queue.push (thread);}
  }
  sleep_queue = temp_queue;
}

void enableInterrupts() {
  if (sigemptyset(&scheduler.sa_mask) != 0){
    ending_lib = true;
    fprintf(stderr,INTERRUPT_ERROR_MESSAGE);
    end_lib();
    exit(EXIT_FAILURE);
  }
  
}

void disableInterrupts() {
  if (sigaddset(&scheduler.sa_mask, SIGVTALRM) != 0){
    ending_lib = true;
    fprintf(stderr,INTERRUPT_ERROR_MESSAGE);
    end_lib();
    exit(EXIT_FAILURE);
  }
}

bool is_ready_queue_empty() {
  return ready_queue.empty();
}

void start_time() {
  if (setitimer(ITIMER_VIRTUAL, &timer, NULL)) {
    ending_lib = true;
    fprintf(stderr,SET_TIME_ERROR_MESSAGE);
    end_lib();
    exit(EXIT_FAILURE);
  }
}

void reset_time() {
  if (setitimer(ITIMER_VIRTUAL, 0, 0)) {
    ending_lib = true;
    fprintf(stderr,SET_TIME_ERROR_MESSAGE);
    end_lib();
    exit(EXIT_FAILURE);
  }
}

Thread *get_thread (int tid)
{
  if (global_current_thread->tid == tid)
    return global_current_thread;

  //Search in blockedq
  if(blocked_map.find (tid) != blocked_map.end()){
    return blocked_map.find (tid)->second;
  }

  //Search in readyq
  std::queue<Thread*> temp_queue_0;
  Thread* return_thread = NULL;
  while (!ready_queue.empty()) {
    Thread* current_thread = ready_queue.front();
    ready_queue.pop();
    if (current_thread->tid == tid) {return_thread = current_thread;}
    temp_queue_0.push(current_thread);
  }
  ready_queue = temp_queue_0;
  if(return_thread != NULL){return return_thread;}

  //Search in sleeping
  std::queue<Thread*> temp_queue_1;
  while (!sleep_queue.empty()) {
    Thread* current_thread = sleep_queue.front();
    sleep_queue.pop();
    if (current_thread->tid == tid) {return_thread = current_thread;}
    temp_queue_1.push(current_thread);
  }
  sleep_queue = temp_queue_1;
  if(return_thread != NULL){return return_thread;}

  return NULL;
}

void free_thread(Thread* thread){
  free (thread->stack);
  free (thread);
}

void out_thread (int tid)
{
  //Search in blockedq
  if(blocked_map.find (tid) != blocked_map.end()){
     blocked_map.erase (tid);
  }

  //Search in readyq
  std::queue<Thread*> temp_queue_0;
  Thread* return_thread = NULL;
  while (!ready_queue.empty()) {
    Thread* current_thread = ready_queue.front();
    ready_queue.pop();
    if (current_thread->tid == tid) {return_thread = current_thread;}
    else{temp_queue_0.push(current_thread);}
  }
  ready_queue = temp_queue_0;
  if(return_thread != NULL){return ;}

  //Search in sleeping
  std::queue<Thread*> temp_queue_1;
  while (!sleep_queue.empty()) {
    Thread* current_thread = sleep_queue.front();
    sleep_queue.pop();
    if (current_thread->tid == tid) {return_thread = current_thread;}
    else{temp_queue_1.push(current_thread);}
  }
  sleep_queue = temp_queue_1;
}

// Function to perform thread context switch
//next thred in is place
void context_switch(Thread* this_thread,Thread* next_thread) {
  reset_time();
  global_current_thread = next_thread;
  int ret = sigsetjmp(env[this_thread->tid], 1);
  if(ret == 1){
    enableInterrupts();
    start_time();
    }
  else{
    global_current_thread -> state = RUNNING;
    total_quantums += 1;
    next_thread->quantum_count += 1;
    update_sleep_queue();
    lib_push (this_thread);
    clean_deleted_thread();   
    enableInterrupts();
    start_time();
    siglongjmp(env[next_thread->tid],1);
  }
}

// Signal handler for timer interrupts
void timer_handler(int sig) {
  if (ending_lib || is_ready_queue_empty()) {
    global_current_thread->quantum_count += 1;
    total_quantums += 1;
    return;
  }
  disableInterrupts();
  Thread* this_thread = global_current_thread;
  this_thread ->state = READY;
  Thread* next_thread = get_next_thread();
  context_switch(this_thread,next_thread);
  reset_time();
  start_time();
}

bool check_valid_ID(int tid){
  if (tid < 0 || tid >= MAX_THREAD_NUM) {
    fprintf (stderr,INVALID_THREAD_ID_ERROR_MASSAGE);
    return true;
  }
  return false;
}

// Function to get the ID of the next available thread
int get_next_tid() {
  if (id_queue.empty()){return LIB_ERROR;}
  int id = id_queue.top();
  id_queue.pop();
  return id;
}

/**
 * @brief initializes the thread library.
 *
 * Once this function returns, the main thread (tid == 0) will be set as RUNNING. There is no need to
 * provide an entry_point or to create a stack for the main thread - it will be using the "regular" stack and PC.
 * You may assume that this function is called before any other thread library function, and that it is called
 * exactly once.
 * The input to the function is the length of a quantum in micro-seconds.
 * It is an error to call this function with non-positive quantum_usecs.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_init(int quantum_usecs){
  //check input
  if (quantum_usecs <= 0){
    fprintf (stderr,QUANTUM_ERROR_MASSAGE);
    return LIB_ERROR;
  }
  // Initialize global variables
  global_quantum_usecs = quantum_usecs;
  for (int i = 1; i < MAX_THREAD_NUM; ++i)
  {
    id_queue.push (i);
  }
  //start scheduler
  scheduler.sa_handler = &timer_handler;
  scheduler.sa_flags = SA_RESTART;
  sigemptyset(&scheduler.sa_mask);
  sigaddset(&scheduler.sa_mask, SIGVTALRM);
  if (sigaction(SIGVTALRM, &scheduler, NULL) != 0) {
    fprintf(stderr,SIG_ACK_ERROR_MESSAGE);
    //free all
    exit(EXIT_FAILURE);
  }
  //save the main context table
  //and catorize the runing program to be the main Thread lib
  
  Thread* new_thread = (struct Thread*)malloc(sizeof(struct Thread));
  if (new_thread == NULL) {
    ending_lib = true;
    fprintf (stderr, MALLOC_ERROR_MASSAGE);
    exit(EXIT_FAILURE);
  }
  
  new_thread->tid = 0; 
  new_thread->state = RUNNING;
  new_thread->quantum_count = 1;
  total_quantums += 1;
  new_thread->sleeping = 0;
  global_current_thread = new_thread;

  
  new_thread->stack =(char *) malloc (STACK_SIZE);
  if (new_thread->stack == NULL){
    ending_lib = true;
    fprintf (stderr, MALLOC_ERROR_MASSAGE);
    free (new_thread);
    exit(EXIT_FAILURE);
  }

  // Configure the timer to fire signal at intervals of quantum_usecs
  // microseconds
  timer.it_value.tv_sec = global_quantum_usecs / 1000000;
  timer.it_value.tv_usec = global_quantum_usecs % 1000000;
  timer.it_interval = timer.it_value;
  // Start the timer
  start_time();

  return EXIT_SUCCESS;
}

/**
 * @brief Creates a new thread, whose entry point is the function entry_point with the signature
 * void entry_point(void).
 *
 * The thread is added to the end of the READY threads list.
 * The uthread_spawn function should fail if it would cause the number of concurrent threads to exceed the
 * limit (MAX_THREAD_NUM).
 * Each thread should be allocated with a stack of size STACK_SIZE bytes.
 * It is an error to call this function with a null entry_point.
 *
 * @return On success, return the ID of the created thread. On failure, return -1.
*/
int uthread_spawn(thread_entry_point entry_point){
  disableInterrupts();
  if (entry_point == nullptr){
    fprintf (stderr,SPAWN_ENTRY_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }
  int id = get_next_tid();
  if(id == LIB_ERROR){
    fprintf (stderr,SPAWN_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }

  // Create a new thread
  Thread* new_thread = (struct Thread*)malloc(sizeof(struct Thread));
  if (new_thread == NULL) {
    ending_lib = true;
    fprintf (stderr, MALLOC_ERROR_MASSAGE);
    end_lib();
    exit(EXIT_FAILURE);
  }
  new_thread->tid = id; // Assign a unique thread ID
  new_thread->entry_point = entry_point;
  new_thread->quantum_count = 0;
  new_thread->sleeping = 0;
  new_thread->stack =(char *) malloc (STACK_SIZE);
  if (new_thread->stack == NULL){
    ending_lib = true;
    fprintf (stderr, MALLOC_ERROR_MASSAGE);
    free (new_thread);
    end_lib();
    exit(EXIT_FAILURE);
  }

  // initializes env to use the right stack,
  // and to run from the function 'entry_point', when we'll use
  // siglongjmp to jump into the thread.
  address_t sp = (address_t) new_thread->stack  + STACK_SIZE - sizeof(address_t);
  address_t pc = (address_t) entry_point;
  if(sigsetjmp(env[new_thread->tid], 1)) {
    // Error occurred during context save, exit the program
    fprintf (stderr,SIG_JUMP_ERROR_MESSAGE);
    free_thread (new_thread);
    end_lib();
    exit(EXIT_FAILURE);
  }
  (env[new_thread->tid]->__jmpbuf)[JB_SP] = translate_address(sp);
  (env[new_thread->tid]->__jmpbuf)[JB_PC] = translate_address(pc);
  sigemptyset(&env[new_thread->tid]->__saved_mask);

  // Add the new thread to the end of the queue of READY threads
  new_thread->state = READY; // Set the state of the new thread to READY
  ready_queue.push(new_thread);
  enableInterrupts();
  return new_thread->tid;
}

/**
 * @brief Terminates the thread with ID tid and deletes it from all relevant control structures.
 *
 * All the resources allocated by the library for this thread should be released. If no thread with ID tid exists it
 * is considered an error. Terminating the main thread (tid == 0) will result in the termination of the entire
 * process using exit(0) (after releasing the assigned library memory).
 *
 * @return The function returns 0 if the thread was successfully terminated and -1 otherwise. If a thread terminates
 * itself or the main thread is terminated, the function does not return.
*/
int uthread_terminate(int tid){
  disableInterrupts();

  // Check if the thread ID is valid
  if (check_valid_ID (tid)){
    fprintf (stderr,INVALID_THREAD_ID_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;}
  // If tid refers to the main thread, terminate the entire process
  if (tid == 0) {
    //CLEAN SESTME MEMORY
    end_lib();
    exit(EXIT_SUCCESS);
  }
  //Get the terminated thread
  Thread* terminated_thread = get_thread(tid);
  if(terminated_thread == NULL) {
    fprintf (stderr, THREAD_NOT_FOUND_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }
  id_queue.push (terminated_thread->tid);
  terminated_thread->state = TERMINATED;
  if (uthread_get_tid() != tid){
    out_thread(tid);
    enableInterrupts();
    return EXIT_SUCCESS;
  }

  Thread* next_thread = get_next_thread();
  context_switch (terminated_thread,next_thread);
  return EXIT_SUCCESS;
}

/**
 * @brief Blocks the thread with ID tid. The thread may be resumed later using uthread_resume.
 *
 * If no thread with ID tid exists it is considered as an error. In addition, it is an error to try blocking the
 * main thread (tid == 0). If a thread blocks itself, a scheduling decision should be made. Blocking a thread in
 * BLOCKED state has no effect and is not considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_block(int tid){
  disableInterrupts();

  // Check if the thread ID is valid
  if (check_valid_ID (tid)){
    fprintf (stderr,INVALID_THREAD_ID_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;}
  // Check if the specified thread is the main thread
  if (tid == 0) {
    fprintf (stderr,BLOCKED_MAIN_THREAD_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }
  Thread* thread = get_thread (tid);
  if(thread == NULL){
    fprintf (stderr, THREAD_NOT_FOUND_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }
  thread->state = BLOCKED;
  // change the thread with the specified ID to bloced_map if its exists in
  // the ready queue
  std::queue<Thread*> temp_queue;
  while (!ready_queue.empty()) {
    Thread* current_thread = ready_queue.front();
    ready_queue.pop();
    if (current_thread->tid == tid) {
      // If the thread is not already in BLOCKED state, change its state to BLOCKED
      blocked_map[current_thread->tid] = current_thread;
    }else{temp_queue.push(current_thread);}
  }
  // Restore the ready queue
  ready_queue = temp_queue;

  if(global_current_thread->tid != tid){
    enableInterrupts();
    return EXIT_SUCCESS;// Thread successfully blocked
  }
  //we block the runing thread
  Thread* next_thread = ready_queue.front();
  ready_queue.pop();
  //Context Switch
  context_switch(global_current_thread,next_thread);
  return EXIT_SUCCESS;
}

/**
 * @brief Resumes a blocked thread with ID tid and moves it to the READY state.
 *
 * Resuming a thread in a RUNNING or READY state has no effect and is not considered as an error. If no thread with
 * ID tid exists it is considered an error.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_resume(int tid){
  disableInterrupts();
  if (check_valid_ID (tid)){
    fprintf (stderr,INVALID_THREAD_ID_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;}
  Thread* thread = get_thread (tid);
  if(thread == nullptr){
    fprintf (stderr, THREAD_NOT_FOUND_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }

  thread->state = READY;
  if(blocked_map.find (tid) != blocked_map.end()){
    ready_queue.push (blocked_map.find (tid)->second);
    blocked_map.erase (tid);
  }

  enableInterrupts();
  return EXIT_SUCCESS;
}

/**
 * @brief Blocks the RUNNING thread for num_quantums quantums.
 *
 * Immediately after the RUNNING thread transitions to the BLOCKED state a scheduling decision should be made.
 * After the sleeping time is over, the thread should go back to the end of the READY queue.
 * If the thread which was just RUNNING should also be added to the READY queue, or if multiple threads wake up
 * at the same time, the order in which they're added to the end of the READY queue doesn't matter.
 * The number of quantums refers to the number of times a new quantum starts, regardless of the reason. Specifically,
 * the quantum of the thread which has made the call to uthread_sleep isn’t counted.
 * It is considered an error if the main thread (tid == 0) calls this function.
 *
 * @return On success, return 0. On failure, return -1.
*/
int uthread_sleep(int num_quantums){
  disableInterrupts();
  if (num_quantums <= 0){
    fprintf (stderr,QUANTUM_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }
  Thread *this_thread = global_current_thread;
  if(this_thread->tid == 0){
    fprintf (stderr,BLOCKED_MAIN_THREAD_ERROR_MASSAGE);
    enableInterrupts();
    return LIB_ERROR;
  }

  Thread* next_thread = get_next_thread();

  this_thread->sleeping = num_quantums;
  this_thread->state = READY;
  context_switch (this_thread,next_thread);
  return EXIT_SUCCESS;
}

/**
 * @brief Returns the thread ID of the calling thread.
 *
 * @return The ID of the calling thread.
*/
int uthread_get_tid(){
  return global_current_thread->tid;
}


/**
 * @brief Returns the total number of quantums since the library was initialized, including the current quantum.
 *
 * Right after the call to uthread_init, the value should be 1.
 * Each time a new quantum starts, regardless of the reason, this number should be increased by 1.
 *
 * @return The total number of quantums.
*/
int uthread_get_total_quantums(){
  return total_quantums;
}


/**
 * @brief Returns the number of quantums the thread with ID tid was in RUNNING state.
 *
 * On the first time a thread runs, the function should return 1. Every additional quantum that the thread starts should
 * increase this value by 1 (so if the thread with ID tid is in RUNNING state when this function is called, include
 * also the current quantum). If no thread with ID tid exists it is considered an error.
 *
 * @return On success, return the number of quantums of the thread with ID tid. On failure, return -1.
*/
int uthread_get_quantums(int tid){
  if (check_valid_ID (tid)){
    fprintf (stderr,INVALID_THREAD_ID_ERROR_MASSAGE);
    return LIB_ERROR;}
  disableInterrupts();
  Thread* thread = get_thread (tid);
  enableInterrupts();
  if(thread == nullptr){
    fprintf (stderr, THREAD_NOT_FOUND_ERROR_MASSAGE);
    return LIB_ERROR;
  }
  return thread->quantum_count;
}

void end_lib(){
  ending_lib = true;
  clean_deleted_thread();
  while (!sleep_queue.empty()){
    Thread* thread = sleep_queue.front();
    sleep_queue.pop();
    free_thread (thread);
    out_thread(thread->tid);
  }
  for (const auto &item: blocked_map){
    Thread* thread = item.second;
    blocked_map.erase (thread->tid);
    free_thread (thread);
    out_thread(thread->tid);
  }
  while (!ready_queue.empty()){
    Thread* thread = ready_queue.front();
    ready_queue.pop();
    free_thread (thread);
    out_thread(thread->tid);
  }
  free_thread (global_current_thread);

}

Thread* get_next_thread(){
  Thread* next_thread = ready_queue.front();
  while (next_thread ->state == TERMINATED){
    next_thread = ready_queue.front();
  }
  ready_queue.pop();
  return next_thread;
}

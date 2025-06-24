This project implements a user-level thread library in C++.

## Features

* **Thread Creation and Management:** Create, terminate, block, and resume user-level threads.
* **Scheduling:** Uses a round-robin preemptive scheduler based on a timer interrupt.
* **Thread States:** Threads can exist in `RUNNING`, `READY`, `BLOCKED`, or `TERMINATED` states.
* **Sleeping Threads:** Supports putting threads to sleep for a specified number of quantums.
* **Quantum Management:** Tracks total quantums and quantums run by each individual thread.
* **Error Handling:** Includes robust error checking for various scenarios like invalid thread IDs, null entry points, and resource limitations.

## How it Works

The library utilizes `setjmp` and `longjmp` for context switching between user-level threads. A `SIGVTALRM` signal is used to trigger the scheduler at regular intervals defined by the `quantum_usecs` parameter during initialization.

### Key Components:

* **`Thread` Structure:** Represents a thread, storing its ID, state, stack, entry point, and quantum count.
* **`ready_queue`:** A queue holding threads in the `READY` state, waiting to be scheduled.
* **`blocked_map`:** A map storing threads in the `BLOCKED` state, accessible by their thread ID.
* **`sleep_queue`:** A queue for threads that are temporarily sleeping.
* **`id_queue`:** A priority queue to manage available thread IDs.
* **`global_current_thread`:** A pointer to the currently running thread.
* **`timer_handler`:** The signal handler responsible for context switching and updating thread states when a quantum expires.
* **`context_switch`:** The core function that saves the current thread's context and restores the next thread's context.

## Public API

* `int uthread_init(int quantum_usecs)`: Initializes the thread library.
* `int uthread_spawn(thread_entry_point entry_point)`: Creates a new thread.
* `int uthread_terminate(int tid)`: Terminates a thread.
* `int uthread_block(int tid)`: Blocks a thread.
* `int uthread_resume(int tid)`: Resumes a blocked thread.
* `int uthread_sleep(int num_quantums)`: Puts the calling thread to sleep.
* `int uthread_get_tid()`: Returns the ID of the calling thread.
* `int uthread_get_total_quantums()`: Returns the total number of quantums since initialization.
* `int uthread_get_quantums(int tid)`: Returns the number of quantums a specific thread has run.

## Compilation and Usage

**(Instructions for compilation and usage would typically go here, e.g., how to compile the `uthreads.cpp` and `demo_jmp.c` files and link them with an example program.)**

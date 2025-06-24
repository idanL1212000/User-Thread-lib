# User-Level Thread Library

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

To compile the `uthreads` library, navigate to the directory containing the `uthreads.cpp`, `demo_jmp.c`, and `Makefile` files, and run the following command:

```bash
make
```

This will create libuthreads.a, the static library.

To clean up the generated object files and the library, use:

```bash
make clean
```
To create a tarball for submission, run:

```bash
make tar
```
This will create ex2.tar containing README, uthreads.cpp, demo_jmp.c, and Makefile.

Integrating with Your Application
To use this library in your own C++ application, you will need to:

Include the Header: Ensure uthreads.h (assuming such a header exists or you define the function prototypes yourself) is accessible in your application's source files.
Compile Your Application: Compile your application's source files.
Link with the Library: Link your compiled application with libuthreads.a.
Here's an example of how you might compile and link a simple application (e.g., main.cpp) that uses the uthreads library:

```bash
g++ -Wall -std=c++11 -g main.cpp -L. -luthreads -o my_app
```
main.cpp: Your application's main source file.
-L.: Tells the linker to look for libraries in the current directory (.).
-luthreads: Tells the linker to link with libuthreads.a (the lib prefix and .a suffix are implied).
-o my_app: Specifies the output executable name as my_app.
Remember to replace main.cpp with the actual name of your application's source file.

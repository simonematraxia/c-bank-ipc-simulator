# C Bank IPC Simulator

**Course**: Operating Systems  
**Authors**: Simone Giovanni Matraxia, Paolo Francesco Garrasi  

## Overview
This repository contains a multithreaded client-server application developed in C for a UNIX/Linux environment. It was built to simulate a banking system where multiple clients can securely connect to a central server to manage their accounts, check balances, and execute money transfers.

The project heavily focuses on **Inter-Process Communication (IPC)**, network programming, and safe concurrency management.

## Core Features
* **Client-Server Architecture**: Implements reliable TCP/IP network communication using POSIX sockets.
* **Multithreading**: The server handles multiple concurrent client connections asynchronously using `pthreads`.
* **Shared Memory (System V)**: Utilizes `shmget` and `shmat` to load and maintain the bank's user database in RAM, allowing fast cross-process access.
* **Message Queues**: Implements System V message queues (`msgget`, `msgsnd`) for an asynchronous, centralized server logging system.
* **Concurrency Synchronization**: Implements **Mutexes** and Condition Variables to prevent race conditions and ensure data consistency during concurrent transactions.
* **File I/O**: Parses a `users.csv` file via a dedicated child process at startup to populate the shared memory with user credentials and initial balances.

## File Structure
* `server.c`: The core server logic managing client connections, thread pooling, and shared memory access.
* `client.c`: The terminal-based client application handling user inputs and socket communication.
* `users.c`: Helper process to parse the `.csv` database and load it into shared memory.
* `users.csv`: Mock database containing user IDs, credentials, and bank balances.
* `Makefile`: Build automation script for compiling the project.

## Technical Stack
* **Language**: C
* **Environment**: Linux / Ubuntu (WSL compatible)
* **Key Libraries**: `<sys/socket.h>`, `<pthread.h>`, `<sys/ipc.h>`, `<sys/shm.h>`, `<sys/msg.h>`

## How to Build and Run
*Note: This project requires a UNIX-like environment (Linux or macOS) to compile due to the POSIX libraries used.*

1. Ensure all source files and the `users.csv` database are in the same directory.
2. Compile the project using the provided Makefile:
   ```bash
   make
   ```
3. Start the Server:
   ```bash
   ./server
   ```
4. Start the Client (Open a new terminal window):
   ```bash
   ./client
   ```
5. Clean executable files when done:
   ```bash
   make clean
   ```

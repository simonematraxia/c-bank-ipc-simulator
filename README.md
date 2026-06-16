# C Bank IPC Simulator

**Course**: Operating Systems  
**Author**: Simone Giovanni Matraxia  

## Overview
This repository contains a multithreaded client-server application developed in C for a UNIX/Linux environment. It was built as a university project for the "Operating Systems" course to simulate a banking system where multiple clients can securely connect to a central server to manage their accounts, check balances, and execute money transfers.

The project heavily focuses on **Inter-Process Communication (IPC)**, network programming, and safe concurrency management.

## Core Features
* **Client-Server Architecture**: Implements reliable TCP/IP network communication using POSIX sockets.
* **Multithreading**: The server handles multiple concurrent client connections asynchronously using `pthreads`.
* **Shared Memory**: Utilizes System V shared memory (`shmget`, `shmat`) to load and maintain the bank's user database in RAM, allowing fast access across the system.
* **Concurrency Synchronization**: Implements **Mutexes** and Condition Variables to prevent race conditions and ensure data consistency during concurrent transactions (e.g., simultaneous money transfers).
* **File I/O**: Parses a `users.csv` file at startup to populate the shared memory with user credentials and initial bank balances.

## File Structure
* `server.c`: The core server logic managing client connections, thread pooling, and shared memory access.
* `client.c`: The terminal-based client application handling user inputs and socket communication.
* `users.c`: Helper functions to parse and load the `.csv` database.
* `users.csv`: Mock database containing user IDs, credentials, and bank balances.

## Technical Stack
* **Language**: C
* **Environment**: Linux / Ubuntu (WSL compatible)
* **Key Libraries**: `<sys/socket.h>`, `<pthread.h>`, `<sys/ipc.h>`, `<sys/shm.h>`

## How to Build and Run
*Note: This project requires a UNIX-like environment (Linux or macOS) to compile due to the POSIX libraries used.*

1. Ensure all `.c` files and the `.csv` database are in the same directory.
2. Compile the source code using standard GCC:
   ```bash
   gcc server.c users.c -o server -lpthread
   gcc client.c -o client
   ```
3. Start the Server:
   ```bash
   ./server
   ```
4. Start the Client (Open a new terminal window):
   ```bash
   ./client
   ```

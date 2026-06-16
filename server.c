#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#define BUFFER_SIZE 1024
#define FIELDSIZE 64
#define MAX_USERS 10
#define MAX_CONNECTIONS 5
#define PORT 8080
#define LOG_MSG_SIZE 256
#define LOG_MSG_TYPE 1
#define LOG_KEY_FILE "/tmp/server_log_queue"


int connections_counter = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char error_message[LOG_MSG_SIZE];

typedef struct Users {
    char username[FIELDSIZE];
    char password[FIELDSIZE];
    int balance;
} Users;

typedef struct Transaction {
    char from[FIELDSIZE];
    char to[FIELDSIZE];
    int amount;
} Transaction;

struct log_msg_buffer {
    long msg_type;
    char msg_text[LOG_MSG_SIZE];
};

Users *loaded_users;
int log_msgid; // ID of the message queue for logs

void *client_handler(void *arg);
void handle_transaction(char *buffer, int client_socket);
int handle_login(char *credentials, int client_socket);
void write_csv(Users users[], int user_counter);
void handle_signal(int signo);
void sigchld_handler(int signo);
void segfault_handler(int signo);
void init_log_queue();
void log_event(const char *event_msg, ...);
void print_logs();

pthread_t client_threads[MAX_CONNECTIONS];
int main() {

    int child_pid;
    pthread_t server_thread;
    
    init_log_queue();
    signal(SIGINT, handle_signal);
    signal(SIGCHLD, sigchld_handler);

    key_t key = ftok("/tmp", 'a');
    int shmid = shmget(key, sizeof(Users) * MAX_USERS, IPC_CREAT | 0666);
    if(shmid == -1) {
        perror("SHMGET");
        exit(EXIT_FAILURE);
    
    }

    loaded_users = (Users *)shmat(shmid, NULL, 0);

    if((child_pid = fork()) == -1) {
        perror("Fork failed");
        exit(EXIT_FAILURE);
    }

    if (child_pid == 0) {
        execlp("./users", "user", (char *)NULL);
        perror("execlp");
        exit(EXIT_FAILURE);
    } else {
        wait(NULL);
        
        if (loaded_users == (Users *)(-1)) {
            perror("SHMAT");
            exit(EXIT_FAILURE);
        }

        int server_socket, client_socket;
        if((server_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
            perror("ERROR");
            exit(EXIT_FAILURE);
        }

        if(setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) == -1) {
            perror("Error could not modify socket options");
            exit(EXIT_FAILURE);
        }

        struct sockaddr_in server_addr, client_addr;
        socklen_t addr_size = sizeof(struct sockaddr_in);

        memset(&server_addr, 0, sizeof(server_addr));
        server_addr.sin_family = AF_INET;
        server_addr.sin_addr.s_addr = INADDR_ANY;
        server_addr.sin_port = htons(PORT);

        if(bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1 ) {
            perror("Unable to bind socket");
            exit(EXIT_FAILURE);
        }
        
        if(listen(server_socket, MAX_CONNECTIONS) == -1) {
            perror("Unable to listen for incoming connections");
            exit(EXIT_FAILURE);
        }

        printf("\n++ SERVER STARTED ++\n");
        log_event("Server successfully started.");

        sleep(1);
        printf("\n Waiting for incoming connections...\n\n");

        int client_sockets[MAX_CONNECTIONS];
        while(1) {
            if((client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &addr_size)) != -1) {
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &(client_addr.sin_addr), client_ip, INET6_ADDRSTRLEN);
                printf("* Accepted connection from: %s.\n", client_ip);
                log_event("Connection: %s", client_ip);

                pthread_mutex_lock(&mutex);
                client_sockets[connections_counter] = client_socket;
                connections_counter += 1;
                pthread_mutex_unlock(&mutex);
                
                
                if(pthread_create(&client_threads[connections_counter-1], NULL, client_handler, (void *)&client_sockets[connections_counter-1]) != 0) {
                    perror("Unable to create new client thread");
                    exit(EXIT_FAILURE);
                    close(client_socket);
                }
            }
        }
    }

    return 0;
}

void *client_handler(void *arg) {
    int client_socket = *(int *)arg;
    int bytes_received;
    char buffer[BUFFER_SIZE];
    char credentials[BUFFER_SIZE];
    int authorized = 0;

    signal(SIGSEGV, segfault_handler);
    
    do{
        bytes_received = recv(client_socket, credentials, sizeof(credentials)-1, 0);
        buffer[bytes_received] = '\0';
        if(bytes_received == -1) {
                perror("ERROR RECV");
                exit(EXIT_SUCCESS);
            }
        authorized = handle_login(credentials, client_socket);
    }while(authorized != 1);

    while ((bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0)) > 0) {
        buffer[bytes_received] = '\0';
        pthread_mutex_lock(&mutex);
        handle_transaction(buffer,client_socket);
        write_csv(loaded_users, MAX_USERS);
        pthread_mutex_unlock(&mutex);
    }
    if(bytes_received == 0){
        printf("\n+++ Client disconnected. +++\n");
        log_event("Disconnection occurred.");
    } else if(bytes_received == -1) {
        perror("Error receiving data");
        snprintf(error_message, LOG_MSG_SIZE, "%s", strerror(errno));
        log_event("Error recieving data: %s", error_message);
    }
    

    close(client_socket);
    pthread_exit(NULL);
}

int handle_login(char *credentials, int client_socket) {
    char *username = strtok(credentials, ":");
    char *password = strtok(NULL, ":");

    if(username != NULL && password != NULL) {
        for(int i = 0; i < MAX_USERS; i++) {
            if(strcmp(username, loaded_users[i].username) == 0) {
                if(strcmp(password, loaded_users[i].password) == 0) {
                    printf("User %s authenticated.\n", username);
                    log_event("Authentication: %s", username);
                    int r;
                    char message[FIELDSIZE];
                    sprintf(message, "%d", loaded_users[i].balance);
                    if ((r = send(client_socket, message, strlen(message), 0)) == -1) {
                        perror("Unable to send status message to client");
                        exit(EXIT_FAILURE);
                    }
                    return 1;
                }
            }
        }
    }
    int r;
    const char *message = "Client is not logged in: login failed.";
    if ((r = send(client_socket, message, strlen(message), 0)) == -1) {
        perror("Unable to send error message to client");
        exit(EXIT_FAILURE);
    }
    log_event("Failed attempt of authentication by: %s", username);
    return 0;
}

void handle_transaction(char *buffer, int client_socket) {
    char to[FIELDSIZE];
    char from[FIELDSIZE];
    int amount;
    int flag = 0;
    strcpy(to, strtok(buffer, ":"));
    strcpy(from, strtok(NULL, ":"));
    amount = atoi(strtok(NULL, ":"));
    int r;
    
    for(int i=0; i<MAX_USERS; i++) {
        if(strcmp(from, loaded_users[i].username) == 0) {
            if(loaded_users[i].balance >= amount) {
                for(int j=0; j<MAX_USERS; j++) {
                    if(strcmp(to, loaded_users[j].username) == 0) {
                        flag = 2;
                        loaded_users[i].balance -= amount;
                        loaded_users[j].balance += amount;
                        printf("\nBank transfer recieved from %s to -> %s, %d.00€ transferred. \n", from, to, amount);
                        printf("\n=== Updated credit === \n %s: %d.00€ \n %s: %d.00€ \n======================\n", from, loaded_users[i].balance, to, loaded_users[j].balance);
                        
                        log_event("Money transfer: %s -> %s , %d.00€", from, to, amount);
                    }
                }    
            } else {
                flag=1;
                break;
            }
        }
        if(flag==2) break;
    }

    if(flag == 0) {
        const char *message = "Operation failed: Recipient user does not exist.";
        if((r = send(client_socket, message, strlen(message), 0)) == -1) {
            perror("Unable to send error message to client");
            exit(EXIT_FAILURE);
        }
        log_event("Failed operation from %s: 'User %s not found.'", from, to);
    } else if(flag == 1){
        const char *message = "Operation failed: Insufficient funds.";
        if((r = send(client_socket, message, strlen(message), 0)) == -1) {
            perror("Unable to send error message to client");
            exit(EXIT_FAILURE);
        }
        log_event("Failed operation from %s of %d.00€: 'Insufficient money in the balance.'", from, amount);
    } else if (flag == 2){
        const char *message = "Operation successful: Funds transferred successfully.";
        if((r = send(client_socket, message, strlen(message), 0)) == -1) {
            perror("Unable to send error message to client");
            exit(EXIT_FAILURE);
        }
    }
}

void write_csv(Users users[], int user_counter) {
    const char *filename = "users.csv";
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("ERROR opening file");
        exit(EXIT_FAILURE);
    }
    for (int i = 0; i < user_counter; i++) {
        fprintf(fp, "%s,%s,%d\n", users[i].username, users[i].password, users[i].balance);
    }
    fclose(fp);
}

void handle_signal(int signo) {
    if(signo == SIGINT) {
        printf("\nCtrl+C signal detected. Closing the server...\n");
        log_event("Server successfully closed.");
        print_logs();
        
        if(shmdt(loaded_users) == -1) {
            perror("SHMDT");
            exit(EXIT_FAILURE);
        }

        if (msgctl(log_msgid, IPC_RMID, NULL) == -1) {
            perror("msgctl");
            exit(EXIT_FAILURE);
        }

        for(int i=0; i<connections_counter; i++) {
            pthread_join(client_threads[i], NULL);
        }
        exit(EXIT_SUCCESS);
    }
}

void sigchld_handler(int signo) {
    int status;
    pid_t pid;

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
        if (WIFEXITED(status)) {
            printf("Child process %d exited with status %d\n", pid, WEXITSTATUS(status));
        } else if (WIFSIGNALED(status)) {
            printf("Child process %d terminated by signal %d\n", pid, WTERMSIG(status));
        }
    }
}

void segfault_handler(int signo) {
    printf("Thread %lu received SIGSEGV signal.\n", pthread_self());
    log_event(" @@@ Warning! @@@ Thread %lu received SIGSEGV signal.", pthread_self());
    exit(EXIT_FAILURE);
}

void init_log_queue() {
    key_t log_key;

    FILE *key_file = fopen(LOG_KEY_FILE, "w");
    if (key_file == NULL) {
        perror("Error creating key file");
        exit(EXIT_FAILURE);
    }
    fclose(key_file);

    if ((log_key = ftok(LOG_KEY_FILE, 'L')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    if ((log_msgid = msgget(log_key, 0644 | IPC_CREAT)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }
    //printf("Message queue ID: %d\n", log_msgid);
}
    

void log_event(const char *event_msg, ...) {
    struct log_msg_buffer log_msg;
    va_list args;
    va_start(args, event_msg);
    
    time_t current_time;
    struct tm *local_time;
    time(&current_time);
    local_time = localtime(&current_time);
    
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", local_time);
    
    snprintf(log_msg.msg_text, sizeof(log_msg.msg_text), "[%s] @", timestamp);
    
    vsnprintf(log_msg.msg_text + strlen(timestamp) + 4, sizeof(log_msg.msg_text) - strlen(timestamp) - 4, event_msg, args);
    
    //printf("%s\n", log_msg.msg_text);

    // Inserting the log message into the queue
    log_msg.msg_type = LOG_MSG_TYPE;
    if (msgsnd(log_msgid, &log_msg, sizeof(struct log_msg_buffer) - sizeof(long), 0) == -1) {
    perror("msgsnd");
    }


    va_end(args);
}

void print_logs() {
    struct log_msg_buffer log_msg;

    // Reading and printing logs from the queue
    printf("\nOutput log since the server has been running :\n (@: server log || #: users log)\n");
    while (msgrcv(log_msgid, &log_msg, sizeof(log_msg.msg_text), LOG_MSG_TYPE, IPC_NOWAIT) != -1) {
        printf("%s\n", log_msg.msg_text);
    }
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>
#include <stdarg.h>
#include <errno.h>
#include <time.h>

#define FIELDSIZE 64
#define MAX_USERS 10
#define MAX_LINE_SIZE 1024
#define LOG_MSG_TYPE 1
#define SERVER_KEY_FILE "/tmp/server_log_queue"
#define LOG_MSG_SIZE 256
#define MAX_ERROR_MESSAGE_LENGTH 256


typedef struct Users {
    char username[FIELDSIZE];
    char password[FIELDSIZE];
    int balance;
} Users;

struct log_msg_buffer {
    long msg_type;
    char msg_text[LOG_MSG_SIZE];
};

int read_csv(const char *filename, Users users[]);
void detach_shared_memory(Users *users);
void log_event(const char *event_msg, ...);

int log_msgid;
char error_message[MAX_ERROR_MESSAGE_LENGTH];

int main() {

    key_t server_key;
    int server_msgid;
    struct log_msg_buffer message;
    
    if ((server_key = ftok(SERVER_KEY_FILE, 'L')) == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    if ((server_msgid = msgget(server_key, 0644)) == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    log_msgid = server_msgid;

    log_event("Program successfully started and log queue achieved.");

    key_t key = ftok("/tmp", 'a');
    int shmid = shmget(key, sizeof(Users) * MAX_USERS, IPC_CREAT | 0666);
    if(shmid == -1) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s", strerror(errno));
        log_event("Shmget error occurred: %s ", error_message);
        exit(EXIT_FAILURE);
    }
    log_event("Shared memory achieved.");

    Users *stored_users = (Users *)shmat(shmid, NULL, 0);
    if(stored_users == (Users *)(-1)) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s", strerror(errno));
        log_event("Shmat error occurred: %s ", error_message);
        exit(EXIT_FAILURE);
    }
    log_event("Shared memory attached.");
    
    const char *filename = "users.csv";
    int user_counter = read_csv(filename, stored_users);
    log_event("Users loaded.");
    /*
    for (int i = 0; i < user_counter; i++) {
        printf("Username: %s.\nPassword: %s.\nBalance: %d.\n", stored_users[i].username, stored_users[i].password, stored_users[i].balance);
        printf("\n");
    }
    */

    detach_shared_memory(stored_users);
    log_event("Shared memory detached.");

    log_event("Program terminated successfully.");
    return 0;
}

int read_csv(const char *filename, Users users[]) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s", strerror(errno));
        log_event("File opening error occurred: %s ", error_message);
        exit(EXIT_FAILURE);
    }

    int num_users = 0;

    char line[MAX_LINE_SIZE];
    while (fgets(line, MAX_LINE_SIZE, fp) != NULL) {
        char *token = strtok(line, ",");

        if (num_users < MAX_USERS) {
            strcpy(users[num_users].username, token);
            token = strtok(NULL, ",");

            strcpy(users[num_users].password, token);
            token = strtok(NULL, ",");

            users[num_users].balance = atoi(token);

            num_users += 1;
        } else {
            log_event(" ### Warning! ### Exceeded max number of users, only the first %d users will be loaded.", MAX_USERS);
            break;
        }
    }
            
    fclose(fp);
    return num_users;
}

void detach_shared_memory(Users *users) {
    if(shmdt(users) == -1) {
        snprintf(error_message, MAX_ERROR_MESSAGE_LENGTH, "%s", strerror(errno));
        log_event("Shmdt error occurred: %s ", error_message);
        exit(EXIT_FAILURE);
    }
            
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
    
    snprintf(log_msg.msg_text, sizeof(log_msg.msg_text), "[%s] #", timestamp);
    
    vsnprintf(log_msg.msg_text + strlen(timestamp) + 4, sizeof(log_msg.msg_text) - strlen(timestamp) - 4, event_msg, args);

    log_msg.msg_type = LOG_MSG_TYPE;
    if (msgsnd(log_msgid, &log_msg, sizeof(struct log_msg_buffer) - sizeof(long), 0) == -1) {
    perror("msgsnd");
    }


    va_end(args);
}
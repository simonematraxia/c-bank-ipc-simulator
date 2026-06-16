#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <signal.h>

#define FIELDSIZE 64
#define BUFFER_SIZE 1024
#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 8080

int shared_variable = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

char username[FIELDSIZE];
char password[FIELDSIZE];

void *login_handler(void *arg);
void *transactions_handler(void *arg);
void handle_signal(int signo);
void segfault_handler(int signo);

typedef struct Transaction {
    char from[FIELDSIZE];
    char to[FIELDSIZE];
    int amount;
} Transaction;

int main() {
    pthread_t login_thread, transaction_thread;
    int client_socket;
    struct sockaddr_in server_addr;
    
    signal(SIGINT, handle_signal);
    
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Error: socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr) <= 0) {
        perror("Error: invalid address");
        exit(EXIT_FAILURE);
    }

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Error: connection failed");
        exit(EXIT_FAILURE);
    }

    pthread_create(&login_thread, NULL, login_handler, (void *)&client_socket);
    pthread_create(&transaction_thread, NULL, transactions_handler, (void *)&client_socket);

    pthread_join(login_thread, NULL);
    pthread_join(transaction_thread, NULL);

    return 0;
}

void *login_handler(void* arg) {    
    int client_socket = *(int *)arg;

    signal(SIGSEGV, segfault_handler);

    int flag = 1;
    do {
        printf("---------------------LOGIN---------------------\n");
        printf("* Username: ");
        fgets(username, sizeof(username), stdin);
        username[strcspn(username, "\n")] = '\0';
        printf("* Password: ");
        fgets(password, sizeof(password), stdin);
        password[strcspn(password, "\n")] = '\0';
        printf("-----------------------------------------------\n");

        char log[BUFFER_SIZE + strlen(username) + strlen(password)];
        strcpy(log, username);
        strcat(log, ":");
        strcat(log, password);
        size_t total_sent = 0;
        size_t id_lenght = strlen(log);
        ssize_t bytes_sent;

        while(total_sent < id_lenght) {

            bytes_sent = send(client_socket, log + total_sent, id_lenght - total_sent, 0);

            if(bytes_sent == -1) {
                perror("Unable to send data");
                exit(EXIT_FAILURE);
            }
            total_sent += bytes_sent;
        }
        ssize_t bytes_received;
        char recieve[BUFFER_SIZE];

        if((bytes_received = recv(client_socket, recieve, sizeof(recieve)-1, 0)) == 0) {
            printf("Connection closed by the server.\n");
            exit(EXIT_SUCCESS);
        }
        recieve[bytes_received] = '\0';
        

        const char *message = "Client is not logged in: login failed.";
        if(bytes_received > 0) {
            if(strcmp(recieve, message) != 0) {
                flag = 0;
                printf("\n +++ Welcome %s. Your current balance is: %s.00€ +++\n", username, recieve);
            } else {
                printf("\n%s\n", recieve);
            }
        }
        
    } while (flag == 1);

    pthread_mutex_lock(&mutex);
    shared_variable = 1;
    pthread_cond_signal(&condition);
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

void *transactions_handler(void* arg) {
    int client_socket = *(int *)arg;

    signal(SIGSEGV, segfault_handler);

    pthread_mutex_lock(&mutex);

    while (shared_variable == 0) {
        pthread_cond_wait(&condition, &mutex);
    }

    Transaction transaction;
    char risposta[10];   
    do{
        printf("-----------------------TRANSFER-----------------------\n");
        printf("Recipient: ");
        scanf("%s", transaction.to);
        printf("Amount: ");
        scanf("%d", &transaction.amount);
        strcpy(transaction.from, username);
        printf("--------------------------------------------------------\n");

        char buffer[BUFFER_SIZE];
        strcpy(buffer, transaction.to);
        strcat(buffer, ":");
        strcat(buffer, transaction.from);
        strcat(buffer, ":");
        sprintf(buffer + strlen(buffer), "%d", transaction.amount);


        size_t total_sent = 0;
        size_t message_length = strlen(buffer);
        ssize_t bytes_sent;

        while (total_sent < message_length) {
        if((bytes_sent = send(client_socket, buffer + total_sent, message_length-total_sent, 0)) == -1) {
            perror("Unable to send transaction details to server");
            exit(EXIT_FAILURE);
        }
        total_sent += bytes_sent;
        }

        ssize_t bytes_received;
        if((bytes_received = recv(client_socket, buffer, sizeof(buffer)-1, 0)) == 0) {
            printf("Connection closed by the server.\n");
            exit(EXIT_SUCCESS);
        }
        buffer[bytes_received] = '\0';

        if(bytes_received > 0) {
            FILE *datePipe = popen("date" , "r");
                if(datePipe == NULL) {
                    perror("Error opening a pipe");
                }
                    
                char pipe_buffer[128];
                while(fgets(pipe_buffer, sizeof(pipe_buffer), datePipe) != NULL) {
                    printf("%.*s -> ", 20, pipe_buffer);
                }

                if(pclose(datePipe) == -1) {
                    perror("Error closing pipe");
                }

            printf("%s\n", buffer);
         }

        printf("Do you want to make another transfer before closing the application?  [yes/no] \t");
        scanf("%s", risposta);
    }while(strcmp(risposta, "yes") == 0);

    printf("\n######### THANK YOU FOR CHOOSING IN.C #########\n");
    pthread_mutex_unlock(&mutex);

    pthread_exit(NULL);
}

void handle_signal(int signo) {
    if(signo == SIGINT) {
        printf("\nCtrl+C signal detected. Closing the client...\n");
        printf("\n######### THANK YOU FOR CHOOSING IN.C #########\n");
        exit(EXIT_SUCCESS);
    }
}

void segfault_handler(int signo) {
    printf("Thread %lu received SIGSEGV signal.\n", pthread_self());
    exit(EXIT_FAILURE);
}
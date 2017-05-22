//
// Created by Delsin on 05/15/2017.
//
/* A simple server in the internet domain using TCP
The port number is passed as an argument


 To compile: gcc server.c -o server
*/
// TODO
// 1. ERRO message 40 bytes
// 2. Change code for SOLN, WORK
//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include "log.h"
#include "sha256.h"
#include "uint256.h"



#define MAX_CLIENTS 100
#define MAX_MSG_SIZE 256
#define PING 0
#define PONG 1
#define OKAY 2
#define ERRO 3
#define SOLN 4
#define WORK 5
#define ABRT 6
#define INVALID 7



typedef struct work_s {
    struct work_s* prev;
    struct work_s* next;

    // These 4 attributes have fixed length
    BYTE target[32];
    BYTE seed[81];
    BYTE solution[17];
    BYTE diff[9];

    char client_ip[INET_ADDRSTRLEN];
    int numworker;
    int fd;
} Work;



int load_options(int argc, char** argv);
void* connection_handler(void* client_sockfd);
char* get_response(char* client_msg, size_t msg_len);
int id_msg(char* msg, size_t msg_len);
int handle_soln(char* client_msg);
void handle_work_msg(char* msg, int sock, char* ip);
void handle_abrt_msg(int sock, char* ip);
void abort_work(int sock);
void remove_elements(int sock);
void enqueue(Work* work);
Work* dequeue();
void print_queue();
void* work_processor(void* none);



pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;
Work* work_queue = NULL;
Work* queue_end = NULL;
Work* active_work_pointer;
int active_work;



int main(int argc, char **argv)
{
    int sockfd, client_sockfd, portno, clilen;

    struct sockaddr_in serv_addr, cli_addr;

    /* Create TCP socket */
    portno = load_options(argc, argv);
    start_log();
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("ERROR opening socket");
        exit(1);
    }
    puts("Socket created.");


    /* Create address we're going to listen on (given port number)
     - converted to network byte order & any IP address for
     this machine */
    bzero((char *) &serv_addr, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);  // store in machine-neutral format

    /* Bind address to the socket */
    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("ERROR on binding");
        exit(1);
    }
    puts("Binding done.");

    /* Listen on socket - means we're ready to accept connections -
     incoming connection requests will be queued */
    listen(sockfd, MAX_CLIENTS);
    puts("Start listening.");
    clilen = sizeof(cli_addr);

    /* Thread to handle WORK messages */
    pthread_mutex_lock(&work_mutex);
    active_work = 0;
    pthread_mutex_unlock(&work_mutex);
    pthread_t worker;
    pthread_create(&worker, NULL, work_processor, NULL);


    while(1) {
        client_sockfd = accept(sockfd, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen);

        if(client_sockfd < 0) {
            perror("ERROR on accept");
            continue;
        }

        // Hand the connection to a new thread
        puts("Connection accepted");
        int* new_sock = malloc(1);
        *new_sock = client_sockfd;
        pthread_t thread_id;
        pthread_create(&thread_id, NULL, connection_handler, (void*) new_sock);
        puts("Handler assigned");
    }
}



int load_options(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    } else {
        return atoi(argv[1]);
    }
}



void* connection_handler(void* client_sockfd) {
    pthread_detach(pthread_self());

    int sock = *(int*)client_sockfd;
    char ip[INET_ADDRSTRLEN];
    strcpy(ip, get_ip(sock));
    log_connection(sock, ip);

    char response[MAX_MSG_SIZE], client_msg[MAX_MSG_SIZE];
    int read_size;

    // Receive message from client
    while((read_size = recv(sock, client_msg, MAX_MSG_SIZE, 0)) > 0) {
        // Get client message
        client_msg[read_size] = '\0'; // Mark end of string
        log_client_msg(sock, client_msg, ip);

        // Handle client message
        int msg_type = id_msg(client_msg, strlen(client_msg));
        if(msg_type == PING) {
            strcpy(response, "PONG\r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == PONG) {
            strcpy(response, "ERRO PONG is reserved for server\r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == OKAY) {
            strcpy(response, "ERRO OKAY is reserved for server\r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == ERRO) {
            strcpy(response, "ERRO ERRO is reserved for server\r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == SOLN) {
            int pass = handle_soln(client_msg);
            if (pass == 1) {
                strcpy(response, "OKAY\r\n");
            } else {
                strcpy(response, "ERRO invalid solution\r\n");
            }
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == WORK) {
            handle_work_msg(client_msg, sock, ip);
        } else if (msg_type == ABRT) {
            handle_abrt_msg(sock, ip);
            print_queue();
        } else {
            strcpy(response, "ERRO invalid message\r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        }

        // Clear message buffer
        bzero((char*)&response, sizeof(response));
        bzero((char*)&client_msg, sizeof(client_msg));
    }

    if (read_size == 0) {
        abort_work(sock);
        log_disconnection(sock, ip);
        close(sock);
        puts("client disconnected.");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("ERROR recv failed.");
    }

    pthread_exit(0);
    //return 0;
}



char* get_response(char* client_msg, size_t msg_len) {
    //char* response;
    if(msg_len < 6) { // If message too short, it can't be valid
        return "ERRO invalid message\r\n";
    } else {
        if (strncmp(client_msg, "PING", 4) == 0) { // Potential PING
            if(msg_len == 6 && client_msg[4] == '\r' && client_msg[5] == '\n') {
                return "PONG\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "PONG", 4) == 0) { // Potential PONG
            if(msg_len == 6 && client_msg[4] == '\r' && client_msg[5] == '\n') {
                return "ERRO PONG is reserved for server\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "OKAY", 4) == 0) { // Potential OKAY
            if(msg_len == 6 && client_msg[4] == '\r' && client_msg[5] == '\n') {
                return "ERRO OKAY is reserved for server\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "ERRO", 4) == 0) { // Potential ERRO
            if(msg_len == 6 && client_msg[4] == '\r' && client_msg[5] == '\n') {
                return "ERRO ERRO is reserved for server\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "SOLN", 4) == 0) { // Potential SOLN
            if(msg_len == 97 && client_msg[95] == '\r' && client_msg[96] == '\n') { // SOLN message fixed len 97
                int pass = handle_soln(client_msg);
                if(pass == 1) {
                    return "OKAY\r\n";
                } else {
                    return "ERRO not a solution\r\n";
                }
//                return "This is a SOLN message\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "WORK", 4) == 0) { // Potential WORK
            if(msg_len == 100 && client_msg[98] == '\r' && client_msg[99] == '\n') { // WORK message fixed len 100
                return "This is a WORK message";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else if (strncmp(client_msg, "ABRT", 4) == 0) { // Potential ABRT
            if(msg_len == 6 && client_msg[4] == '\r' && client_msg[5] == '\n') {
                return "OKAY\r\n";
            } else {
                return "ERRO invalid message\r\n";
            }
        } else {
            return "ERRO invalid message\r\n"; // All other headers are invalid
        }
    }
}



/* Identify type of message */
int id_msg(char* msg, size_t msg_len) {
    if(msg_len < 6) { // If message too short, it can't be valid
        return INVALID;
    } else {
        if (strncmp(msg, "PING", 4) == 0) { // Potential PING
            if(msg_len == 6 && msg[4] == '\r' && msg[5] == '\n') {
                return PING;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "PONG", 4) == 0) { // Potential PONG
            if(msg_len == 6 && msg[4] == '\r' && msg[5] == '\n') {
                return PONG;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "OKAY", 4) == 0) { // Potential OKAY
            if(msg_len == 6 && msg[4] == '\r' && msg[5] == '\n') {
                return OKAY;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "ERRO", 4) == 0) { // Potential ERRO
            if(msg_len == 6 && msg[4] == '\r' && msg[5] == '\n') {
                return ERRO;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "SOLN", 4) == 0) { // Potential SOLN
            if(msg_len == 97 && msg[95] == '\r' && msg[96] == '\n') { // SOLN message fixed len 97
                return SOLN;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "WORK", 4) == 0) { // Potential WORK
            if(msg_len == 100 && msg[98] == '\r' && msg[99] == '\n') { // WORK message fixed len 100
                return WORK;
            } else {
                return INVALID;
            }
        } else if (strncmp(msg, "ABRT", 4) == 0) { // Potential ABRT
            if(msg_len == 6 && msg[4] == '\r' && msg[5] == '\n') {
                return ABRT;
            } else {
                return INVALID;
            }
        } else {
            return INVALID; // All other headers are invalid
        }
    }
}



void handle_work_msg(char* msg, int sock, char* ip) {
    BYTE seed[81];
    BYTE solution[17];
    BYTE numworker[3];
    bzero(seed,64);
    bzero(solution,16);
    bzero(numworker,3);
    memcpy(seed, msg + 14, 64);
    seed[64] = '\0';
    memcpy(solution, msg + 79, 16);
    solution[16] = '\0';
    memcpy(numworker, msg + 96, 2);
    int decnumwork = (int)strtol((char*)numworker, NULL, 16);
    //printf("seed %s solution %s\n",seed,solution);
    //get target
    BYTE dif[9];//difficulty
    memcpy(dif, msg + 5, 8);
    dif[8] = '\0';
    //printf("difficulty : %s\n",dif);
    BYTE alpha[3];
    BYTE beta[7];
    memcpy(alpha,dif,2);
    alpha[2] = '\0';
    memcpy(beta,dif+2,6);
    beta[6] = '\0';
    //printf("alpha : %s\n",alpha);
    //printf("beta : %s\n",beta);
    int al = (int)strtol((char*)alpha, NULL, 16);
    //int be = (int)strtol((char*)beta, NULL, 16);
    //printf("alpha : %d\n",al);
    //printf("beta : %d\n",be);
    int pow = 8 * (al - 3);
    //uint64_t target = be * pow(2, 8 * (al - 3));
    //printf("power : %d\n",pow);
    BYTE target[32];
    uint256_init(target);
    BYTE b1[3],b2[3],b3[3];
    memcpy(b1,beta,2);
    b1[2] = '\0';
    memcpy(b2,beta + 2,2);
    b2[2] = '\0';
    memcpy(b3,beta + 4,2);
    b3[2] = '\0';
    //printf("b1 : %s,b2 : %s, b3 : %s\n",b1,b2,b3);
    target[29] = (BYTE)strtol((char*)b1, NULL, 16);
    target[30] = (BYTE)strtol((char*)b2, NULL, 16);
    target[31] = (BYTE)strtol((char*)b3, NULL, 16);
    //printf("%x %x %x\n",target[29],target[30],target[31]);
    //print_uint256(target);
    BYTE res[32];
    uint256_init(res);
    uint256_sl(res,target,pow);

    Work* wk = malloc(sizeof(Work));
    strcpy(wk->client_ip, ip);
    wk->prev = NULL;
    wk->next = NULL;
    wk->fd = sock;
    wk->numworker = decnumwork;
    int i;
    for(i = 0;i < 32;i++){
        wk->target[i] = res[i];
    }
    strcpy((char*)(wk->diff),(char*)dif);
    strcpy((char*)(wk->seed),(char*)seed);//81, 16 reserve for nonce
    strcpy((char*)(wk->solution),(char*)solution);
    pthread_mutex_lock(&queue_mutex);
    enqueue(wk);
    print_queue();
    pthread_mutex_unlock(&queue_mutex);
}



void handle_abrt_msg(int sock, char* ip) {
    abort_work(sock);
    char* response = "OKAY\r\n";
    write(sock, response, strlen(response));
    log_server_msg(sock, response, ip);
}



void abort_work(int sock) {
    pthread_mutex_lock(&work_mutex);
    pthread_mutex_lock(&queue_mutex);
    remove_elements(sock);
    if(active_work_pointer!= NULL) {
        if(active_work_pointer->fd == sock) {
            active_work = 0;
            active_work_pointer = NULL;
        }
    }
    pthread_mutex_unlock(&queue_mutex);
    pthread_mutex_unlock(&work_mutex);
}



void remove_elements(int sock) {
    Work* temp = work_queue;
    Work* target = NULL;
    while (temp != NULL) {
        if (temp->fd == sock) { // This is the work we want to remove from queue.
            if(temp == work_queue) { // Probe is head of queue
                target = temp;
                work_queue = work_queue->next;
                if (work_queue != NULL) { // Queue still not empty
                    work_queue->prev = NULL;
                } else {
                    queue_end = NULL;
                }
                temp = work_queue;
                free(target);
            } else if (temp == queue_end) { // Probe is end of queue
                target = temp;
                queue_end = queue_end->prev;
                if (queue_end != NULL) { // Queue still not empty
                    queue_end->next = NULL;
                } else {
                    work_queue = NULL;
                }
                temp = NULL;
                free(target);
            } else { // Probe is at mid of queue
                target = temp;
                temp->prev->next = temp->next;
                temp->next->prev = temp->prev;
                temp = temp->next;
                free(target);
            }
        } else { // This is not what we are looking for
            temp = temp->next;
        }
    }
}



void enqueue(Work* work) {
    if (work_queue == NULL) { // If work queue is empty
        work_queue = work;
        queue_end = work;
        work->prev = NULL;
        work->next = NULL;
    } else {
        queue_end->next = work;
        work->prev = queue_end;
        work->next = NULL;
        queue_end = work;
    }
}



Work* dequeue() {
    if (work_queue == NULL) { // If work queue empty
        return NULL;
    } else {
        Work* temp = work_queue;
        work_queue = work_queue->next;
        if (work_queue != NULL) { // If queue still not empty
            work_queue->prev = NULL;
        } else { // If queue finally empty
            queue_end = NULL;
        }
        temp->prev = NULL;
        temp->next = NULL;
        return temp;
    }
}



void print_queue() {
    Work* temp = work_queue;
    int n = 0;
    while(temp != NULL) {
        n += 1;
        print_uint256(temp->target);
        printf("seed %s\n",temp->seed);
        printf("solution %s\n",temp->solution);
        printf("=============================\n");
        temp = temp->next;
    }
    printf("<<<<<<<< Total %d works queued >>>>>>>>\n", n);
}



void* work_processor(void* none) {
    pthread_detach(pthread_self());

    (void)none;//stop warning message
    Work* work;
    while(1){
        pthread_mutex_lock(&work_mutex);
        pthread_mutex_lock(&queue_mutex);
        if(active_work == 0){
            //printf("before dequeue : %d\n",print());
            work = dequeue();
            if(work != NULL){
                //printf("after dequeue : %d\n",print());
                active_work_pointer = work;
                active_work = 1;
            }
        }
        pthread_mutex_unlock(&queue_mutex);
        pthread_mutex_unlock(&work_mutex);
        if(work != NULL){
            //convert nonce
            int i;
            BYTE res2[32];
            BYTE nonce[32];//actually it is 16 in length
            BYTE one[32];
            uint256_init(res2);
            uint256_init(nonce);
            uint256_init(one);
            one[31] = 0x1;
            //print_uint256(one);
            //print_uint256(nonce);
            for(i = 0;i < 16;i+=2){
                char temp[3];
                temp[2] = '\0';
                strncpy(temp,(char*)(work->solution + i),2);
                int number = (int)strtol(temp, NULL, 16);
                nonce[24 + i/2] = number;
            }
            //print_uint256(nonce);
            char soln[17] = "0000000000000000";
            strcat((char*)work->seed,soln);

            //printf("work->seed %s\n",(char*)work->seed);// seed | nonce
            //printf("length %d\n",strlen((char*)work->seed));//expecting 80


            int index;

            BYTE nseed[40];
            for (index = 0; index < 40; nseed[index++] = 0);//init

            for(index = 0; index < 80;index+=2){
                char temp[3];
                temp[2] = '\0';
                strncpy(temp,(char*)(work->seed + index),2);
                int number = (int)strtol(temp, NULL, 16);
                nseed[index/2] = number;
            }
            //printf("work nseed : %s\n",work->seed);
            /*
            for(i = 0;i < 40;i++){
                printf("%02x", nseed[i]);
            }printf("\n");
            */
            work->seed[64] = '\0';
            BYTE last32[32];
            strncpy((char*)last32,(char*)(nseed) + 8,32);
            int found = 0;
            //print_uint256(work->target);
            while(!found && active_work == 1){// && count < ffffffffffffffff - start
                //+1
                uint256_add(nonce,nonce,one);
                uint256_add(res2,last32,nonce);
                for(index = 0;index < 32;index++){
                    nseed[index + 8] = res2[index];
                }
                /*
                for(i = 0;i < 40;i++){
                    printf("%02x", nseed[i]);
                }
                printf("\n");*/

                SHA256_CTX ctx;
                BYTE buffer1[SHA256_BLOCK_SIZE];
                BYTE buffer2[SHA256_BLOCK_SIZE];
                uint256_init(buffer1);
                uint256_init(buffer2);
                sha256_init(&ctx);
                sha256_update(&ctx, nseed, 40);
                sha256_final(&ctx, buffer1);
                //print_uint256(buffer1);
                sha256_init(&ctx);
                sha256_update(&ctx, buffer1, 32);
                sha256_final(&ctx, buffer2);
                int pass = memcmp(work->target, buffer2, SHA256_BLOCK_SIZE);
                //print_uint256(buffer2);
                //printf("pass value %d\n",pass);
                if(pass > 0){
                    found = 1;
                    char reply[98];//include "\r\n\0"
                    strcpy(reply , "SOLN ");
                    strcat(reply,(char*)work->diff);
                    strcat(reply," ");
                    strcat(reply,(char*)work->seed);
                    strcat(reply," ");
                    char stringnonce[17];
                    stringnonce[16] = '\0';
                    int i;
                    //print_uint256(nonce);
                    int length = 0;
                    for(i = 0;i < 8;i++){
                        int number = *(nonce + i + 24);
                        length += sprintf(stringnonce + length, "%02x", number);
                    }
                    strcat(reply,stringnonce);
                    strcat(reply,"\r\n");
                    write(work->fd,reply,strlen(reply));

                    log_server_msg(work->fd, reply, work->client_ip);
                    printf("server(%d) : %s",work->fd,reply);
                    pthread_mutex_lock(&work_mutex);
                    active_work = 0;
                    pthread_mutex_unlock(&work_mutex);
                    free(work);
                }
            }
        }
    }
}



int handle_soln(char* client_msg) {
    char msg[MAX_MSG_SIZE];
    strcpy(msg, client_msg);
    //get x for H(H(x))
    BYTE seed[81];
    BYTE solution[17];
    bzero(seed,81);
    bzero(solution,17);
    memcpy(seed,msg + 14,64);
    seed[64] = '\0';
    memcpy(solution,msg + 79,16);
    solution[16] = '\0';
    //printf("seed %s solution %s\n",seed,solution);
    strcat((char*)seed,(char*)solution);
    printf("x %s\n",seed);
    BYTE nseed[40];
    int index;
    for (index = 0; index < 40; nseed[index++] = 0);
    for(index = 0; index < 80;index+=2){
        char temp[2];
        strncpy(temp,(char*)(seed + index),2);
        int number = (int)strtol(temp, NULL, 16);
        nseed[index/2] = number;
    }
    print_uint256(nseed);//last 8 byte is not printed
    //get target
    BYTE dif[9];//difficulty
    memcpy(dif,msg + 5,8);
    dif[8] = '\0';
    //printf("difficulty : %s\n",dif);
    BYTE alpha[3];
    BYTE beta[7];
    memcpy(alpha,dif,2);
    alpha[2] = '\0';
    memcpy(beta,dif+2,6);
    beta[6] = '\0';
    //printf("alpha : %s\n",alpha);
    //printf("beta : %s\n",beta);
    int al = (int)strtol((char*)alpha, NULL, 16);
    //int be = (int)strtol((char*)beta, NULL, 16);
    //printf("alpha : %d\n",al);
    //printf("beta : %d\n",be);
    int pow = 8 * (al - 3);
    //uint64_t target = be * pow(2, 8 * (al - 3));
    //printf("power : %d\n",pow);
    BYTE target[64];
    uint256_init(target);
    BYTE b1[3],b2[3],b3[3];
    memcpy(b1,beta,2);
    b1[2] = '\0';
    memcpy(b2,beta + 2,2);
    b2[2] = '\0';
    memcpy(b3,beta + 4,2);
    b3[2] = '\0';
    //printf("b1 : %s,b2 : %s, b3 : %s\n",b1,b2,b3);
    target[29] = (BYTE)strtol((char*)b1, NULL, 16);
    target[30] = (BYTE)strtol((char*)b2, NULL, 16);
    target[31] = (BYTE)strtol((char*)b3, NULL, 16);
    //printf("%x %x %x\n",target[29],target[30],target[31]);
    //print_uint256(target);
    BYTE res[64];
    uint256_init(res);
    uint256_sl(res,target,pow);
    printf("target : ");
    print_uint256(res);
    //res stores target, nseed stores seed | nonce
    SHA256_CTX ctx;
    BYTE buffer1[SHA256_BLOCK_SIZE];
    BYTE buffer2[SHA256_BLOCK_SIZE];
    uint256_init(buffer1);
    uint256_init(buffer2);
    sha256_init(&ctx);
    //printf("nseed size : %d\n",(int)strlen((char*)nseed));
    sha256_update(&ctx, nseed, 40);
    sha256_final(&ctx, buffer1);
    print_uint256(buffer1);
    //printf("buffer1 size : %d\n",(int)strlen((char*)buffer1));
    sha256_init(&ctx);
    sha256_update(&ctx, buffer1, 32);
    sha256_final(&ctx, buffer2);
    int pass = memcmp(res, buffer2, SHA256_BLOCK_SIZE);
    print_uint256(buffer2);
    //printf("buffer2 size : %d\n",(int)strlen((char*)buffer2));
    printf("pass value %d\n",pass);
    if(pass > 0){
        return 1;
        //return "OKAY\r\n";
    }else{
        //return "ERRO\r\n";
        return 0;
    }
}


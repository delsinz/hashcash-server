/*
 name: Mingyang Zhang
 login: mingyangz
 id: 650242
*/


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>
#include <signal.h>
#include "server.h"
#include "log.h"



pthread_mutex_t queue_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t work_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_count_mutex = PTHREAD_MUTEX_INITIALIZER;
Work* work_queue = NULL;
Work* queue_end = NULL;
Work* active_work_pointer;
int active_work;
int client_count;



int main(int argc, char **argv) {
    signal(SIGPIPE, SIG_IGN);

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

    /* Count number of connected clients */
    pthread_mutex_lock(&client_count_mutex);
    client_count = 0;
    pthread_mutex_unlock(&client_count_mutex);

    /* Thread to handle WORK messages */
    pthread_mutex_lock(&work_mutex);
    active_work_pointer = NULL;
    active_work = 0;
    pthread_mutex_unlock(&work_mutex);
    pthread_t worker;
    pthread_create(&worker, NULL, work_processor, NULL);


    while(1) {
        if (client_count < MAX_CLIENTS) {
            client_sockfd = accept(sockfd, (struct sockaddr*) &cli_addr, (socklen_t*) &clilen);

            if(client_sockfd < 0) {
                perror("ERROR on accept");
                continue;
            }

            // Hand the connection to a new thread
            //puts("Connection accepted");
            int* new_sock = malloc(1);
            *new_sock = client_sockfd;
            pthread_t thread_id;
            pthread_create(&thread_id, NULL, connection_handler, (void*) new_sock);
            //puts("Handler assigned");
            // Increment the number of clients
            pthread_mutex_lock(&client_count_mutex);
            client_count ++;
            pthread_mutex_unlock(&client_count_mutex);
        }
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
    signal(SIGPIPE, SIG_IGN);

    int sock = *(int*)client_sockfd;
    char ip[INET_ADDRSTRLEN];
    //strcpy(ip, get_ip(sock));
    get_ip(ip, sock);
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
            strcpy(response, "ERRO PONG is reserved for server           \r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == OKAY) {
            strcpy(response, "ERRO OKAY is reserved for server           \r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == ERRO) {
            strcpy(response, "ERRO ERRO is reserved for server           \r\n");
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == SOLN) {
            int pass = handle_soln(client_msg);
            if (pass == 1) {
                strcpy(response, "OKAY\r\n");
            } else {
                strcpy(response, "ERRO invalid solution                      \r\n");
            }
            write(sock, response, strlen(response));
            log_server_msg(sock, response, ip);
        } else if (msg_type == WORK) {
            handle_work_msg(client_msg, sock, ip);
        } else if (msg_type == ABRT) {
            handle_abrt_msg(sock, ip);
            //print_queue();
        } else {
            strcpy(response, "ERRO invalid message                       \r\n");
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
        
        // Decrease number of clients
        pthread_mutex_lock(&client_count_mutex);
        client_count --;
        pthread_mutex_unlock(&client_count_mutex);

        //puts("client disconnected.");
        fflush(stdout);
    } else if (read_size == -1) {
        perror("ERROR recv failed.");
    }

    pthread_exit(0);
    //return 0;
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
    if (queue_len() < MAX_WORKS) {
        // Extract alpha, beta as byte
        char difficulty[9], alpha[3], beta[7];
        difficulty[8] = '\0';
        alpha[2] = '\0';
        beta[6] = '\0';
        memcpy(difficulty, msg + 5, 8);
        memcpy(alpha, difficulty, 2);
        memcpy(beta, difficulty + 2, 6);

        // Compute alpha as int
        int alpha_int = (int)strtol(alpha, NULL, 16);
        int power = (alpha_int - 3) * 8;

        // Parse beta in byte form
        char byte1[3], byte2[3], byte3[3];
        BYTE beta_byte[32];
        byte1[2] = '\0';
        byte2[2] = '\0';
        byte3[2] = '\0';
        memcpy(byte1, beta, 2);
        memcpy(byte2, beta + 2, 2);
        memcpy(byte3, beta + 4, 2);
        uint256_init(beta_byte);
        beta_byte[29] = (BYTE)strtol(byte1, NULL, 16);
        beta_byte[30] = (BYTE)strtol(byte2, NULL, 16);
        beta_byte[31] = (BYTE)strtol(byte3, NULL, 16);

        // Compute target
        BYTE target[32];
        uint256_init(target);
        uint256_sl(target, beta_byte, power);

        // Extract seed, start, worker count
        BYTE seed_nonce[81], start[17], thread_count_byte[3];
        memset(seed_nonce, 0, 64);
        memset(start, 0, 16);
        memset(thread_count_byte, 0, 3);
        memcpy(seed_nonce, msg + 14, 64);
        seed_nonce[64] = '\0';
        memcpy(start, msg + 79, 16);
        start[16] = '\0';
        memcpy(thread_count_byte, msg + 96, 2);
        int thread_count = (int)strtol((char*)thread_count_byte, NULL, 16);

        // Prepare work for insertion
        Work* work = malloc(sizeof(Work));
        work->sock = sock;
        work->worker_count = thread_count;
        work->prev = NULL;
        work->next = NULL;
        int i;
        for(i = 0; i < 32; i++){
            work->target[i] = target[i];
        }
        strcpy((char*)(work->difficulty),(char*)difficulty);
        strcpy(work->client_ip, ip);
        strcpy((char*)(work->seed_nonce),(char*)seed_nonce);
        strcpy((char*)(work->start),(char*)start);

        // Append work to queue
        pthread_mutex_lock(&queue_mutex);
        append(work);
        //print_queue();
        pthread_mutex_unlock(&queue_mutex);
    } else {
        char* response = "ERRO work queue is full                    \r\n";
        write(sock, response, strlen(response));
        log_server_msg(sock, response, ip);
    }
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
        if(active_work_pointer->sock == sock) {
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
        if (temp->sock == sock) { // This is the work we want to remove from queue.
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



/* Append work to queue */
void append(Work* work) {
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



/* Pop head off queue */
Work* pop() {
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



int queue_len() {
    Work* temp = work_queue;
    int n = 0;
    while (temp != NULL) {
        n += 1;
        temp = temp->next;
    }
    return n;
}



void print_queue() {
    int n = 0;
    Work* temp = work_queue;
    while(temp != NULL) {
        n += 1;
        printf("seed %s\n",temp->seed_nonce);
        printf("solution %s\n",temp->start);
        printf("=============================\n");
        temp = temp->next;
    }
    printf("<<<<<<<< Total %d works queued >>>>>>>>\n", n);
}



void* work_processor(void* none) {
    pthread_detach(pthread_self());
    (void)none;
    signal(SIGPIPE, SIG_IGN);
    Work* work;

    for (;;) {
        // Get a work from queue
        if(active_work == 0){
            pthread_mutex_lock(&queue_mutex);
            work = pop();
            pthread_mutex_unlock(&queue_mutex);
            if(work != NULL){
                pthread_mutex_lock(&work_mutex);
                active_work_pointer = work;
                active_work = 1;
                pthread_mutex_unlock(&work_mutex);
            }
        }

        if(work != NULL) {
            // Get initial nonce
            BYTE nonce[32];
            uint256_init(nonce);
            int i;
            for(i = 0; i < 16; i += 2) {
                char buff[3];
                buff[2] = '\0';
                strncpy(buff, (char*)(work->start + i), 2);
                int int_dec = (int)strtol(buff, NULL, 16);
                nonce[24 + i/2] = int_dec;
            }

            // Prep seed | nonce
            char filler_nonce[17];
            memset(filler_nonce, '0', 17);
            filler_nonce[16] = '\0';
            BYTE seed_nonce[40];
            strcat((char*)work->seed_nonce, filler_nonce);
            int j;
            j = 0;
            while (j < 40) {
                seed_nonce[j] = 0;
                j ++;
            }
            for(j = 0; j < 80; j += 2) {
                char buff[3];
                buff[2] = '\0';
                strncpy(buff, (char*)(work->seed_nonce + j), 2);
                int int_dec = (int)strtol(buff, NULL, 16);
                seed_nonce[j/2] = int_dec;
            }
            work->seed_nonce[64] = '\0';

            // Start searching for solution
            BYTE one[32], bseed_nonce[32], bseed_nonce_base[32];
            uint256_init(one);
            uint256_init(bseed_nonce);
            one[31] = 0x1;
            strncpy((char*)bseed_nonce_base, (char*)(seed_nonce) + 8, 32);
            int found = 0;
            while(!found && active_work == 1){
                // Increment nonce value
                uint256_add(nonce, nonce, one);
                uint256_add(bseed_nonce, bseed_nonce_base, nonce);
                int k;
                for(k = 0; k < 32; k ++){
                    seed_nonce[k + 8] = bseed_nonce[k];
                }

                // Hash it twice
                BYTE hash1[SHA256_BLOCK_SIZE], hash2[SHA256_BLOCK_SIZE];
                SHA256_CTX ctx;
                uint256_init(hash1);
                uint256_init(hash2);
                sha256_init(&ctx);
                sha256_update(&ctx, seed_nonce, HASH1);
                sha256_final(&ctx, hash1);
                sha256_init(&ctx);
                sha256_update(&ctx, hash1, HASH2);
                sha256_final(&ctx, hash2);
                int result = memcmp(work->target, hash2, SHA256_BLOCK_SIZE);

                // Found it!
                if (result > 0) {
                    found = 1;

                    // Construct SOLN message
                    char soln_msg[98], nonce_str[17];
                    nonce_str[16] = '\0';
                    strcpy(soln_msg, "SOLN ");
                    strcat(soln_msg, (char*)work->difficulty);
                    strcat(soln_msg, " ");
                    strcat(soln_msg, (char*)work->seed_nonce);
                    strcat(soln_msg, " ");
                    int len = 0;
                    int l;
                    for(l = 0; l < 8; l++){
                        int int_dec = *(nonce + l + 24);
                        len += sprintf(nonce_str + len, "%02x", int_dec);
                    }
                    strcat(soln_msg, nonce_str);
                    strcat(soln_msg, "\r\n");
                    write(work->sock, soln_msg, strlen(soln_msg));

                    // Logging, cleaning up
                    log_server_msg(work->sock, soln_msg, work->client_ip);
                    pthread_mutex_lock(&work_mutex);
                    active_work_pointer = NULL;
                    active_work = 0;
                    pthread_mutex_unlock(&work_mutex);
                    free(work);
                }
            }
        }
    }
}



int handle_soln(char* client_msg) {
    // Get the client message
    char msg[MAX_MSG_SIZE];
    strcpy(msg, client_msg);

    // Extract alpha, beta
    char difficulty[9], alpha[3], beta[7];
    difficulty[8] = '\0';
    alpha[2] = '\0';
    beta[6] = '\0';
    memcpy(difficulty, msg + 5, 8);
    memcpy(alpha, difficulty, 2);
    memcpy(beta, difficulty+2, 6);

    // Compute alpha as int
    int alpha_int = (int)strtol((char*)alpha, NULL, 16);
    int power = (alpha_int - 3) * 8;

    // Parse beta in byte form
    char byte1[3], byte2[3], byte3[3];
    BYTE beta_byte[32];
    byte1[2] = '\0';
    byte2[2] = '\0';
    byte3[2] = '\0';
    memcpy(byte1, beta, 2);
    memcpy(byte2, beta + 2, 2);
    memcpy(byte3, beta + 4, 2);
    uint256_init(beta_byte);
    beta_byte[29] = (BYTE)strtol(byte1, NULL, 16);
    beta_byte[30] = (BYTE)strtol(byte2, NULL, 16);
    beta_byte[31] = (BYTE)strtol(byte3, NULL, 16);

    // Compute target
    BYTE target[64];
    uint256_init(target);
    uint256_sl(target, beta_byte, power);

    // Reading seed | solution
    BYTE seed_solution[81], solution[17];
    memset(seed_solution, 0, 81);
    memset(solution, 0, 17);
    memcpy(seed_solution, msg + 14, 64);
    seed_solution[64] = '\0'; // This is end of seed
    memcpy(solution, msg + 79, 16);
    solution[16] = '\0'; // This is end of solution
    strcat((char*)seed_solution,(char*)solution); // Put them together

    // Prep seed | nonce for processing
    BYTE seed_nonce[40];
    int i;
    i = 0;
    while(i < 40) {
        seed_nonce[i] = 0;
        i ++;
    }
    for(i = 0; i < 80; i += 2){
        char buff[3];
        buff[2] = '\0';
        strncpy(buff, (char*)(seed_solution + i),2);
        int number = (int)strtol(buff, NULL, 16);
        seed_nonce[i/2] = number;
    }

    // Compare target to double hashed value
    BYTE hash1[SHA256_BLOCK_SIZE], hash2[SHA256_BLOCK_SIZE];
    SHA256_CTX ctx;
    uint256_init(hash1);
    uint256_init(hash2);
    sha256_init(&ctx);
    sha256_update(&ctx, seed_nonce, HASH1);
    sha256_final(&ctx, hash1);
    sha256_init(&ctx);
    sha256_update(&ctx, hash1, HASH2);
    sha256_final(&ctx, hash2);
    int result = memcmp(target, hash2, SHA256_BLOCK_SIZE);
    if(result > 0){
        return 1;
    }else{
        return 0;
    }
}


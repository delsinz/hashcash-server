/*
 name: Mingyang Zhang
 login: mingyangz
 id: 650242
*/

#include "sha256.h"
#include "uint256.h"


#define MAX_CLIENTS 100
#define MAX_WORKS 10
#define MAX_MSG_SIZE 256
#define PING 0
#define PONG 1
#define OKAY 2
#define ERRO 3
#define SOLN 4
#define WORK 5
#define ABRT 6
#define INVALID 7
#define HASH1 40
#define HASH2 32



typedef struct work_s {
    struct work_s* prev;
    struct work_s* next;

    // These 4 attributes have fixed length
    char target[32];
    char seed_nonce[81]; // last 16 bytes are reserved for solution
    char start[17];
    char difficulty[9];

    char client_ip[INET_ADDRSTRLEN];
    int worker_count;
    int sock;
} Work;



int load_options(int argc, char** argv);
void* connection_handler(void* client_sockfd);
int id_msg(char* msg, size_t msg_len);
int handle_soln(char* client_msg);
void handle_work_msg(char* msg, int sock, char* ip);
void handle_abrt_msg(int sock, char* ip);
void abort_work(int sock);
void remove_elements(int sock);
void append(Work* work);
Work* pop();
void print_queue();
int queue_len();
void* work_processor(void* none);

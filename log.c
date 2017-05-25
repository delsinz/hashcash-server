/*
 name: Mingyang Zhang
 login: mingyangz
 id: 650242
*/

#include <stdio.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include "log.h"

#define FILEPATH "log.txt"


FILE* logfile = NULL;
pthread_mutex_t logfile_mutex = PTHREAD_MUTEX_INITIALIZER;



void start_log() {
    lock_logfile();

    if (logfile == NULL) {
        FILE* fp = fopen(FILEPATH, "w");
        if (fp == NULL) {
            perror("ERROR Fail to open logfile");
        } else {
            logfile = fp;
            log_timestamp();
            fprintf(logfile, "sever starting up\n");
            fprintf(logfile, "\n");
        }
        fclose(logfile);
        logfile = NULL;
    } else {
        perror("ERROR Logfile already open");
    }

    unlock_logfile();
}

void lock_logfile() {
    int n = pthread_mutex_trylock(&logfile_mutex);

    // If not locked
    if (n != 0) {
        pthread_mutex_lock(&logfile_mutex);
    }
}

void unlock_logfile() {
    pthread_mutex_unlock(&logfile_mutex);
}

void log_timestamp() {
    struct timeval time;
    int n = gettimeofday(&time, NULL);

    if (n == 0) {
        // Convert seconds to human readable
        time_t seconds = time.tv_sec;
        struct tm* date = localtime(&seconds);

        char buffer[19+1];
        strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", date);

        // Log result
        fprintf(logfile, "%s.%06d\n", buffer, (int)time.tv_usec);
    } else {
        perror("ERROR Getting system time");
        fprintf(logfile, "Fail to get system time");
    }
}

void log_connection(int socket, char* ip) {
    lock_logfile();

    FILE* fp = fopen(FILEPATH, "a");
    if(fp == NULL) {
        perror("ERROR Fail to open logfile");
    } else {
        logfile = fp;
        log_timestamp();
        fprintf(logfile, "client connected\n");
        //log_ip(socket);
        fprintf(logfile, "client IP: %s\n", ip);
        log_socket(socket);
        fprintf(logfile, "\n");
    }
    fclose(logfile);
    logfile = NULL;
    unlock_logfile();
}

void log_disconnection(int socket, char* ip) {
    lock_logfile();

    FILE* fp = fopen(FILEPATH, "a");
    if(fp == NULL) {
        perror("ERROR Fail to open logfile");
    } else {
        logfile = fp;
        log_timestamp();
        fprintf(logfile, "client disconnected\n");
        fprintf(logfile, "client IP: %s\n", ip);
        log_socket(socket);
        fprintf(logfile, "\n");
    }
    fclose(logfile);
    logfile = NULL;
    unlock_logfile();
}

void log_client_msg(int socket, char* msg, char* ip) {
    lock_logfile();

    FILE* fp = fopen(FILEPATH, "a");
    if(fp == NULL) {
        perror("ERROR Fail to open logfile");
    } else {
        logfile = fp;
        log_timestamp();
        fprintf(logfile, "client to server\n");
        //log_ip(socket);
        fprintf(logfile, "client IP: %s\n", ip);
        log_socket(socket);
        log_msg(msg);
        fprintf(logfile, "\n");
    }
    fclose(logfile);
    logfile = NULL;
    unlock_logfile();
}

void log_server_msg(int socket, char* msg, char* ip) {
    lock_logfile();

    FILE* fp = fopen(FILEPATH, "a");
    if(fp == NULL) {
        perror("ERROR Fail to open logfile");
    } else {
        logfile = fp;
        log_timestamp();
        fprintf(logfile, "server to client\n");
        //log_ip(socket);
        fprintf(logfile, "client IP: %s\n", ip);
        log_socket(socket);
        log_msg(msg);
        fprintf(logfile, "\n");
    }
    fclose(logfile);
    logfile = NULL;
    unlock_logfile();
}

//void log_ip(int socket) { // Only used on connection
//    fprintf(logfile, "client IP: %s\n", get_ip(socket));
//}

void get_ip(char* ip_buff, int socket) {
    // use sockaddr_storage to keep address in family-agnostic manner
    struct sockaddr_storage client;
    socklen_t client_size = sizeof client;

    int n = getpeername(socket, (struct sockaddr*)&client, &client_size);

    if(n == 0){

        //IPV4
        char ip[INET_ADDRSTRLEN];

        // Returns null on error
        if(inet_ntop(AF_INET, &client, ip, INET_ADDRSTRLEN)){

            // Success!
            //fprintf(logfile, "Client IP: %s\n", ip);
            strcpy(ip_buff, ip);
            return;

        } else {
            // null! failure
            perror("error presenting ip address with inet_ntop");
            //fprintf(logfile, "unknown ip address");
            strcpy(ip_buff, "unknown");
            return;
        }
    } else {
        perror("error looking up client ip");
        //fprintf(logfile, "unknown ip address");
        strcpy(ip_buff, "unknown");
        return;
    }
}

void log_socket(int socket) {
    fprintf(logfile, "client socket: %d\n", socket);
}

void log_msg(char* msg) {
    fprintf(logfile, "SSTP message: %s\n", msg);
}

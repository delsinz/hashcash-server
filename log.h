/*
 name: Mingyang Zhang
 login: mingyangz
 id: 650242
*/

void start_log();
void lock_logfile();
void unlock_logfile();
void log_timestamp();
void log_connection(int socket, char* ip);
void log_disconnection(int socket, char* ip);
void log_client_msg(int socket, char* msg, char* ip);
void log_server_msg(int socket, char* msg, char* ip);
//void log_ip(int socket);
void get_ip(char* ip_buff, int socket);
void log_socket(int socket);
void log_msg(char* msg);

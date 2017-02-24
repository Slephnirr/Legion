/*
 * Author: Kevin Pack
 */

pthread_mutex_t mutex_changes;

int start_listening(struct sockaddr_in* addr);

int accept_comm(int csock, struct sockaddr_in* addr);

int connect_to_server(const char *dest);

int close_socket(int sock);

void send_int(int num, int sock);

int receive_int(int sock);

void send_char(char c, int sock);

char receive_char(int sock);

void send_uint32_t(uint32_t num, int sock);

uint32_t receive_uint32_t(int sock);

void send_string(const char* str, int sock);

char *receive_string(int sock);

void send_status_entry(struct status_entry *status, int sock);

struct status_entry *receive_status_entry(int sock);

int number_of_status_list_entries(struct status_list_entry *start_list);

void send_status_list(struct status_list_entry **Pstart_list, int sock);

struct status_list_entry *receive_status_list(int sock);

void send_file(const char *path, int sock);

void receive_file(const char *path, int sock);

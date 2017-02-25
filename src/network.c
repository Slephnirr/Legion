/*
 * Author: Kevin Pack
 */

#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <pthread.h>

#include <systemd/sd-journal.h>

#include "entry_control.h"
#include "status_entry.h"
#include "network.h"

#define KRED  "\x1B[31m"
#define RESET "\033[0m"

/*
 * The function creates a socket and binds it to the value of the attribute
 * address_.
 * Furthermore it starts to listen on the created communication socket
 * for an incoming connection and returns the socket.
 */
int start_listening(struct sockaddr_in* addr)
{
        addr->sin_family = AF_INET;
        addr->sin_addr.s_addr = INADDR_ANY;
        addr->sin_port = htons(15000);

        int csock = socket(AF_INET,SOCK_STREAM,0);
        if(csock == -1) {
                sd_journal_print(LOG_ERR, "couldn't create socket!\n");
                exit(EXIT_FAILURE);
        } else {
                if(bind(csock, (struct sockaddr *) addr, sizeof(*addr))!= 0) {
                        sd_journal_print(LOG_ERR, "port busy!\n");
                        exit(EXIT_FAILURE);
                } else {
                        sd_journal_print(LOG_INFO, "port bound...\n");
                        listen(csock, 10);
                        sd_journal_print(LOG_INFO, "start listening...\n");
                        return csock;
                }
        }
}


/*
 * The function takes the communication socket, which the method StartListening
 * returns, accepts the communication and returns if successful the socket.
 */
int accept_comm(int csock, struct sockaddr_in* addr)
{
        socklen_t addrlen = sizeof (struct sockaddr_in);
        int dsock;
        while(1) {
                dsock= accept(csock, (struct sockaddr *) &addr, &addrlen );
                if(dsock > 0)
                        return dsock;

        }
}

/*
 * Receives as input a string of the destination address the method is
 * supposed to connect to.
 * The string address is converted with the function inet_aton() to
 * the attribute sin_addr of the sockaddr_in variable.
 * After a successful connection is established the communication socket
 * is returned.
 */
int connect_to_server(const char *dest)
{
        struct sockaddr_in* dest_addr = calloc(1, sizeof(struct sockaddr_in));
        dest_addr->sin_family = AF_INET;
        dest_addr->sin_port   = htons(15000);
        // the destination string is converted to an address.
        inet_aton(dest, &(dest_addr->sin_addr));
        int sock = socket(AF_INET,SOCK_STREAM,0);
        if(sock == -1) {
                sd_journal_print(LOG_ERR, "couldn't create socket!\n");
                exit(EXIT_FAILURE);
        } else {
                if(connect(sock,(struct sockaddr *) dest_addr,
                                        sizeof(*dest_addr))!= 0) {
                        sd_journal_print(LOG_ERR, "couldn't connect to \
                                                        destination!\n");
                        exit(EXIT_FAILURE);
                } else {
                        return sock;
                }
        }
}

int close_socket(int sock)
{
        return close(sock);
}

void send_int(int num, int sock)
{
        if(write(sock, &num, sizeof(int)) != sizeof(int)) {
                printf("couldn't write to socket!\n");
                exit(EXIT_FAILURE);
        }
}

int receive_int(int sock)
{
        int* Pnum = calloc(1, sizeof(int));
        if (read(sock, Pnum, sizeof(int)) != sizeof(int) ) {
                printf("couldn't read from socket!\n");
                exit(EXIT_FAILURE);
        }
        int num = *Pnum;
        free(Pnum);
        return num;
}

void send_char(char c, int sock)
{
        if(write(sock, &c, sizeof(char)) != sizeof(char)) {
                printf("couldn't write to socket!\n");
                exit(EXIT_FAILURE);
        }
}

char receive_char(int sock)
{
        int *Pc = calloc(1, sizeof(char));
        if (read(sock, Pc, sizeof(char)) != sizeof(char) ) {
                printf("couldn't read from socket!\n");
                exit(EXIT_FAILURE);
        }
        char c = *Pc;
        free(Pc);
        return c;
}


void send_uint32_t(uint32_t num, int sock)
{
        if(write(sock, &num, sizeof(uint32_t)) != sizeof(uint32_t)) {
                printf("couldn't write to socket!\n");
                exit(EXIT_FAILURE);
        }
}

uint32_t receive_uint32_t(int sock)
{
        uint32_t *Pnum = calloc(1, sizeof(uint32_t));
        if (read(sock, Pnum, sizeof(uint32_t)) != sizeof(uint32_t)) {
                printf("couldn't read from socket!\n");
                exit(EXIT_FAILURE);
        }
        uint32_t num = *Pnum;
        free(Pnum);
        return num;
}

void send_string(const char* str, int sock)
{
        int str_len = strlen(str);
        send_int(str_len, sock);
        if(write(sock, str, str_len) != str_len) {
                sd_journal_print(LOG_ERR, "couldn't write to socket!\n");
                exit(EXIT_FAILURE);
        }
}

char *receive_string(int sock)
{
        int str_len = receive_int(sock);
        char *str = calloc((str_len + 1), sizeof(char));
        if(read(sock, str, str_len) != str_len) {
                sd_journal_print(LOG_ERR, "couldn't read from socket!\n");
                exit(EXIT_FAILURE);
        }
        return str;
}

void send_status_entry(struct status_entry *status, int sock)
{
        send_string((status->name), sock);
        send_char((status->type), sock);
        send_char((status->stat), sock);
        send_uint32_t((status->cookie), sock);
}

struct status_entry *receive_status_entry(int sock)
{
        struct status_entry* status = calloc(1, sizeof(struct status_entry));

        status->name = receive_string(sock);
        status->type = receive_char(sock);
        status->stat = receive_char(sock);
        status->cookie = receive_uint32_t(sock);

        return status;
}


int number_of_status_list_entries(struct status_list_entry *start_list)
{
        struct status_list_entry *curr_list = start_list;
        int num = 0;
        while(curr_list != NULL ) {
                num++;
                curr_list = curr_list->next;
        }
        return num;
}

void send_status_list(struct status_list_entry **Pstart_list, int sock)
{
        int num = number_of_status_list_entries(*Pstart_list);
        send_int(num, sock);

        struct status_list_entry *prev_list = NULL;
        struct status_list_entry *curr_list = *Pstart_list;
        struct status_entry *curr_status = NULL;

        for(int i = 1; i <= num; i++) {
                if( i == receive_int(sock)) {
                        curr_status = curr_list->status;
                        send_status_entry(curr_status, sock);

                        pthread_mutex_lock( &mutex_changes );

                        if(prev_list == NULL)
                                *Pstart_list = curr_list->next;
                        else
                                prev_list->next = curr_list->next;

                        delete_status_entry(&curr_status);

                        prev_list = curr_list;
                        curr_list = curr_list->next;

                        free(prev_list);
                        prev_list = NULL;

                        pthread_mutex_unlock( &mutex_changes );
                } else {
                        sd_journal_print(LOG_ERR, "statuslist transmission out\
                                                                   of sync!\n");
                        exit(EXIT_FAILURE);
                }
        }
}

struct status_list_entry *receive_status_list(int sock)
{
        int num = receive_int(sock);

        struct status_list_entry *curr_list;
        struct status_list_entry *start_list = NULL;
        struct status_list_entry *prev_list = start_list;

        for(int i = 1; i <= num; i++) {
                curr_list = calloc(1, sizeof(struct status_list_entry));

                send_int(i, sock);
                curr_list->status = receive_status_entry(sock);

                if(prev_list == NULL) {
                        start_list = curr_list;
                        prev_list = curr_list;
                } else {
                        prev_list->next = curr_list;
                        prev_list = curr_list;
                }
        }
        return start_list;
}


void send_file(const char *path, int sock)
{
        int f_size;
        struct stat* attr = calloc(1, sizeof(struct stat));
        if (stat(path, attr) == 0) {
                f_size =  attr->st_size;
        } else {
                sd_journal_print(LOG_ERR, "couldn't read file.\n");
                exit(EXIT_FAILURE);
        }
        free(attr);

        send_int(f_size, sock);

        int fd;
        if ((fd = open(path, O_RDONLY)) < 0 ) {
                sd_journal_print(LOG_ERR, "can't open file to send!\n");
                exit(EXIT_FAILURE);
        } else {
                int  n;
                int MAX_BUFFER = 1024;
                char buffer[MAX_BUFFER];

                while(f_size > 0) {
                        n = read(fd, buffer, sizeof(buffer));
                        buffer[n] = 0;
                        if(write(sock, buffer, n) != n) {
                                sd_journal_print(LOG_ERR, "couldn't write to\
                                                                  socket!\n");
                                exit(EXIT_FAILURE);
                        }
                        f_size -= n;
                }
                close(fd);
        }
}

void receive_file(const char *path, int sock)
{
        int f_size = receive_int(sock);

        int fd;
        if ((fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644) ) < 0) {
                sd_journal_print(LOG_ERR, "can't open file to write!\n");
                exit(EXIT_FAILURE);
        } else {
                int  n;
                int MAX_BUFFER = 1024;
                char buffer[MAX_BUFFER];
                while(f_size > 0) {
                        n = read(sock, buffer, sizeof(buffer));
                        buffer[n] = 0;
                        if(write(fd, buffer, n) != n) {
                                sd_journal_print(LOG_ERR, "couldn't write to\
                                                                    file!\n");
                                exit(EXIT_FAILURE);
                        }
                        f_size -= n;
                }
                close(fd);
        }
}

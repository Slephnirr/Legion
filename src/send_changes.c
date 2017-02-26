/*
 * Author: Kevin Pack
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>

#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <dirent.h>
#include <pthread.h>

#include <systemd/sd-journal.h>

#include "event_entry.h"
#include "watch_entry.h"
#include "entry_control.h"
#include "status_entry.h"
#include "network.h"
#include "send_changes.h"


#define KGRN  "\x1B[32m"
#define KBLU  "\x1B[34m"
#define RESET "\033[0m"

void list_all_file_entries(struct file_list_entry *start_list)
{
        struct file_list_entry *curr_list = start_list;
        while(curr_list != NULL) {
                printf("%s\n", (curr_list->file)->loc);

                curr_list = curr_list->next;
        }
}

void list_all_dir_entries(struct dir_list_entry *start_list)
{
        struct dir_list_entry *curr_list = start_list;
        while(curr_list != NULL ) {
                struct dir_entry *curr_dir = curr_list->dir;
                printf("%s\n", curr_dir->loc);
                // struct dir_entry* current_subdir_entry =
                //   current_dir_entry -> start_subdir_entries_;
                // while(current_subdir_entry != NULL)
                // {
                //   printf("\t- %s\n", current_subdir_entry -> dir_path_);
                //   current_subdir_entry = current_subdir_entry -> next_dir_entry_;
                // }

                curr_list = curr_list->next;
        }
}

void list_status_entries(struct status_list_entry *start_list, char lswitch)
{
        struct status_list_entry *curr_list = start_list;

        while(curr_list != NULL) {
                struct status_entry *curr_status = curr_list->status;

                char* curr_name = curr_status->name;
                char  curr_type = curr_status->type;
                char  curr_stat = curr_status->stat;

                if(lswitch == 'A')
                        printf("%s -> %c\n", curr_name, curr_stat);

                if((lswitch == 'F') && (curr_type == 'F'))
                        printf("%s -> %c\n", curr_name, curr_stat);

                if((lswitch == 'D') && ( curr_type == 'D' ) )
                        printf("%s -> %c\n", curr_name, curr_stat);

                curr_list = curr_list->next;
        }
}

void list_events(struct event_list_entry *start_list)
{
        struct event_list_entry *curr_list = start_list;
        while(curr_list != NULL ) {
                const struct inotify_event *curr_event = curr_list->event;

                printf("%s ", (curr_event->name));

                if ((curr_event->mask) & IN_CREATE) {
                        if((curr_event->mask) & IN_ISDIR)
                                printf("(C) [directory]\n");
                        else
                                printf("(C) [file]\n");
                }

                if ((curr_event->mask) & IN_DELETE) {
                        if((curr_event->mask) & IN_ISDIR)
                                printf("(D) [directory]\n");
                        else
                                printf("(D) [file]\n");
                }

                if ((curr_event->mask) & IN_CLOSE_WRITE) {
                        if((curr_event->mask) & IN_ISDIR )
                                printf("(W) [directory]\n");
                        else
                                printf("(W) [file]\n");
                }

                if ((curr_event->mask) & IN_MOVED_FROM) {
                        if((curr_event->mask) & IN_ISDIR) {
                                printf("(F) [directory] {%lu}\n",
                                            (curr_event->cookie));
                        } else {
                                printf("(F) [file] {%lu}\n",
                                            (curr_event->cookie));
                        }
                }

                if ((curr_event->mask) & IN_MOVED_TO) {
                        if((curr_event->mask) & IN_ISDIR) {
                                printf("(T) [directory] {%lu}\n",
                                            (curr_event->cookie));
                        } else {
                                printf("(T) [file] {%lu}\n",
                                            (curr_event -> cookie));
                        }
                }
                curr_list = curr_list->next;
        }
}

void list_watch_entries(struct watch_list_entry *start_list)
{
        struct watch_list_entry *curr_list = start_list;
        while(curr_list != NULL ) {
                printf("%s [%d]\n", ((curr_list->watch)->loc),
                                     ((curr_list->watch)->wd));

                curr_list = curr_list->next;
        }
}

// int CountChangesToSubmit(void)
// {
//   struct status_list_entry* curr_list = start_changes_list;
//   int number = 0;
//   while(curr_list != NULL)
//   {
//     number++;
//     curr_list = curr_list -> next;
//   }
//   return number;
// }


int count_files_to_submit(void)
{
        struct status_entry* status;
        struct status_list_entry* prev_list = NULL;
        struct status_list_entry* curr_list = start_changes_list;

        int num = 0;
        while(curr_list != NULL) {
                status = curr_list->status;

                char curr_type = status->type;
                char curr_stat = status->stat;

                if(curr_type == 'F') {
                        if((curr_stat == 'C' ) || (curr_stat == 'W' ))
                                num++;

                        if(curr_stat == 'T') {
                                if(prev_list == NULL) {
                                        num++;
                                } else {
                                        uint32_t curr_cookie = status->cookie;

                                        struct status_entry* prev_status =
                                                          prev_list->status;
                                        uint32_t prev_cookie =
                                                          prev_status->cookie;
                                        // The previous event is no IN_MOVE_FROM
                                        // event. =>
                                        // The file was moved from outside of
                                        // the monitored directory.
                                        if(curr_cookie != prev_cookie)
                                                num++;
                                }
                        }
                }
                prev_list = curr_list;
                curr_list = curr_list->next;
        }
        return num;
}

void provide_updates_for_node(int num, int sock)
{
        for(int i = 1; i <= num; i++) {
                char* str = receive_string(sock);
                int n = does_entry_exist_in_file_system(str);
                send_int(n, sock);

                if(n == 1)
                        send_file(str, sock);
        }
}

int process_update_IN_CREATE(struct status_entry* status, int sock)
{
        char type = status->type;
        char* path = get_full_stat_path(status);
        int num = 0;

        if(type == 'D') {
                mode_t mode = S_IRWXU;
                mkdir(path, mode);

                mode = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE;
                int watch = inotify_add_watch(inotify_instance, path, mode);
        } else {
                send_string(path, sock);
                int exist = receive_int(sock);
                if(exist == 1) {
                        receive_file(path, sock);
                        num++;
                } else {
                        mode_t mode = S_IRWXU;
                        creat(path, mode);
                }
        }
        free(path);
        path = NULL;
        return num;
}

int process_update_IN_DELETE(struct status_entry* status, int sock)
{
        char type = status->type;
        char *path = get_full_stat_path(status);
        int num = 0;

        if(type == 'D') {
                if(does_entry_exist_in_file_system(path) == 1) {
                        rmdir(path);
                        num = 1;
                }
        } else {
                if(does_entry_exist_in_file_system(path) == 1) {
                        remove(path);
                        num = 1;
                }
        }
        free(path);
        path = NULL;
        return num;
}

int process_update_IN_CLOSE_WRITE(struct status_entry* status, int sock)
{
        char type = status->type;
        char *path = get_full_stat_path(status);
        int num = 0;

        send_string(path, sock);
        int exist = receive_int(sock);
        if(exist == 1) {
                receive_file(path, sock);
                num++;
        } else {
                mode_t mode = S_IRWXU;
                creat(path, mode);
        }
        free(path);
        path = NULL;
        return num;
}

int process_update_IN_MOVED_FROM(struct status_list_entry** Pcurr_list, struct
                                             status_entry* status, int sock)
{
        char type = status->type;
        char *path = get_full_stat_path(status);

        struct status_list_entry *curr_list = *Pcurr_list;
        struct status_list_entry *next_list = curr_list->next;
        if(next_list == NULL) {
                free(path);
                path = NULL;
                return 0;
        }

        struct status_entry* next_status = NULL;
        if (next_list != NULL) {
                next_status = next_list->status;
        } else {
                free(path);
                path = NULL;
                return 0;
        }
        // That case is true if entry has been moved outside of the monitored space
        char next_stat = status->stat;
        if (next_stat != 'T') {
                sd_journal_print(LOG_ERR, "next_entry_stat != T");
                exit(EXIT_FAILURE);
        }

        if (((next_status->cookie) == (status->cookie))) {
                char* dest_path = get_full_stat_path(next_status);
                if(rename(path, dest_path) == -1) {
                        sd_journal_print(LOG_ERR,"Couldn't rename entry %s\
                                                                \n!", path);
                        exit(EXIT_FAILURE);
                }
                free(dest_path);
                // Next list entry is the corresponding IN_MOVED_TO which has
                // already been handled now. So it can be skipped.
                *Pcurr_list = (*Pcurr_list)->next;

                free(path);
                path = NULL;
                return 2;
        }
        free(path);
        path = NULL;
        return -1;
}

// int ProcessUpdateINMOVEDTO(struct status_list_entry** Pcurrent_list_entry,
//   status_entry* status_entry, int data_socket)
// {
//   char  status_entry_type = status_entry -> entry_type_;
//   char* path = get_full_stat_path(status_entry);
//
//   int num = 0;
//   if( status_entry_type == 'D')
//   {
//     *Pcurrent_list_entry = (*Pcurrent_list_entry) -> next;
//
//     printf("\nMUTEX_CHANGES: LOCK\n");
//     pthread_mutex_lock(&mutex_changes);
//
//     RemoveStatusEntryInList(&start_updates_list, status_entry);
//
//     printf("MUTEX_CHANGES: UNLOCK\n");
//     pthread_mutex_unlock(&mutex_changes);
//   }
//   else
//   {
//     printf("Requesting file %s [T]...\n", path);
//     SendString(path, data_socket);
//     int does_entry_exist = ReceiveInteger(data_socket);
//     if(does_entry_exist == 1)
//     {
//       ReceiveFile(path, data_socket);
//       num++;
//     }
//     else
//     {
//       mode_t mode = S_IRWXU;
//       creat(path, mode);
//     }
//
//     status_entry -> entry_stat_ = 'C';
//   }
//
//   free(path);
//   path = NULL;
//   return num;
// }


int process_updates_on_node(int sock)
{
        struct status_entry *status;
        struct status_list_entry *curr_list = start_updates_list;
        int num = 0;
        while(curr_list != NULL) {
                status = curr_list->status;
                char curr_stat = status->stat;

                if(curr_stat == 'C')
                        num += process_update_IN_CREATE(status, sock);

                if(curr_stat == 'D') {
                        if(process_update_IN_DELETE(status, sock) == 0) {
                                sd_journal_print(LOG_ERR, "Couldn't delete: %s\
                                                             \n", status->name);
                                exit(EXIT_FAILURE);
                        }
                }
                if(curr_stat == 'W')
                        num += process_update_IN_CLOSE_WRITE(status, sock);

                if(curr_stat == 'F') {
                        int cnum = process_update_IN_MOVED_FROM(&curr_list,
                                                            status, sock);
                        if (cnum == 0 || cnum == 1)
                                continue;
                }

                if(curr_stat == 'T') {
                        sd_journal_print(LOG_ERR, "Single IN_MOVED_TO Event!\n");
                        exit(EXIT_FAILURE);
                }
                curr_list = curr_list->next;
        }
        return num;
}

void *send_changes_thread(void* arg)
{
        int csock;
        int dsock;

        struct sockaddr_in* addr = calloc(1, sizeof(struct sockaddr_in));

        while(1) {
                pthread_mutex_lock(&mutex_SC_thread);
                pthread_cond_wait(&cond_mutex_3, &mutex_SC_thread);
                pthread_mutex_unlock(&mutex_SC_thread);

                while(start_changes_list != NULL) {
                        if (pthread_mutex_trylock(&mutex4) == 0) {
                                if (start_changes_list != NULL) {
                                        int num = count_files_to_submit();

                                        clock_t start, end;
                                        double cpu_time_used;
                                        start = clock();

                                        dsock = connect_to_server(device_ip);

                                        send_status_list(&start_changes_list,
                                                                        dsock);


                                        receive_int(dsock);
                                        send_int(num, dsock);

                                        provide_updates_for_node(num, dsock);

                                        close_socket(dsock);

                                        // start_changes_list = NULL;

                                        end = clock();
                                        cpu_time_used = ((double) (end - start))
                                                                / CLOCKS_PER_SEC;
                                }
                                pthread_mutex_unlock(&mutex4);
                        }
                }
        }
}

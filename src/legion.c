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
#include "legion.h"


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

/*
 * Caution: this code is very messy atm and needs to be rewritten. It's only
 * supposed to work so don't judge it.
 */
void analyse_dir_to_monitor(char* path)
{
        struct dir_entry *new_dir;
        if(does_dir_entry_exist_in_list(path) != 1) {
                int watch = inotify_add_watch(inotify_instance, path,
                        IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE);

                struct watch_entry *new_watch = create_watch_entry(path, watch);
                add_watch_entry_to_list(&start_watch_list, new_watch);

                new_dir = create_dir_entry(path, watch);
                add_dir_entry_to_list(new_dir);
                add_dir_entry_to_parent_dir_entry(new_dir);

                char *loc = (char*) new_dir->loc;
                struct status_entry *status = create_status_entry(loc, 'D',
                                                                        'C', 0);
                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);
        } else {
                new_dir = search_dir_entry_in_list(path);
        }

        struct dir_list_entry *start_subdir = NULL;
        struct dir_list_entry *prev_list = NULL;
        struct dir_list_entry *curr_list = start_subdir;

        DIR *dir_stream;
        if((dir_stream = opendir(path)) != NULL) {
                int cint;
                struct dirent *entry;
                // Read the content of the open directory
                while((entry = readdir(dir_stream)) != NULL) {
                        // Take out the "." and ".." directory links in unix
                        // systems
                        if (strcmp((entry->d_name), ".") != 0 && strcmp(
                                             (entry->d_name),"..") != 0) {
                        	char* full_entry = calloc((strlen(path) + 1 +
                                strlen(entry->d_name) + 1), sizeof(char));
                                strcpy(full_entry, path);
                                strcat(full_entry, "/");
                                strcat(full_entry, (entry->d_name));

                                // 2 for Directories; 1 for Files; 0 unkown error.
                                cint = entry_type(full_entry);
                                // Entry is a Directory.
                                if(cint == 2) {
                                        int watch = inotify_add_watch(
                                                inotify_instance, full_entry,
                                                IN_CREATE | IN_DELETE |
                                                IN_CLOSE_WRITE | IN_MOVE);

                                        struct watch_entry *new_watch =
                                          create_watch_entry(full_entry, watch);
                                        add_watch_entry_to_list(
                                                &start_watch_list, new_watch);

                                        struct dir_entry *new_subdir =
                                        create_dir_entry(full_entry, watch);
                                        add_dir_entry_to_parent_dir_entry(
                                                                new_subdir);
                                        add_dir_entry_to_list(new_subdir);

                                        char* subdir_loc = (char*)new_subdir->loc;
                                        struct status_entry *status =
                                        create_status_entry(subdir_loc, 'D',
                                                                        'C', 0);

                                        pthread_mutex_lock(&mutex_changes);
                                        add_status_entry_to_list(
                                                &start_changes_list, status);
                                        pthread_mutex_unlock(&mutex_changes);

                                        struct dir_list_entry *new_list =
                                        calloc(1, sizeof(struct dir_list_entry));
                                        new_list->dir = new_subdir;
                                        if(curr_list == NULL) {
                                                start_subdir = new_list;
                                                curr_list = start_subdir;
                                        } else {
                                                curr_list->next = new_list;
                                                curr_list = new_list;
                                        }
                                // Entry is a File.
                                } else if (cint == 1) {
                                        struct file_entry *new_file =
                                                  create_file_entry(full_entry);
                                        add_file_entry_to_dir_entry(new_dir,
                                                                   new_file);
                                        add_file_entry_to_list(new_file);

                                        char* loc = (char*)new_file->loc;
                                        struct status_entry *status =
                                        create_status_entry(loc, 'F', 'C', 0);

                                        pthread_mutex_lock(&mutex_changes);
                                        add_status_entry_to_list(
                                                &start_changes_list, status);
                                        pthread_mutex_unlock(&mutex_changes);
                                } else {
                                        sd_journal_print(LOG_ERR, "Couldn't \
                                                determine Nature of Entry.\n");
                                        exit(EXIT_FAILURE);
                                }
                                free(full_entry);
                        }
                }
                closedir(dir_stream);
                curr_list = start_subdir;
                while(curr_list != NULL) {
                        char *path = get_full_dir_path(curr_list->dir);
                        analyse_dir_to_monitor(path);
                        free(path);

                        prev_list = curr_list;
                        curr_list = curr_list->next;

                        prev_list->next = NULL;
                        free(prev_list);
                }

        } else {
                sd_journal_print(LOG_ERR, "Couldn't open directory.\n");
                exit(EXIT_FAILURE);
        }
}

void remove_dir_to_monitor(char* path)
{
        struct dir_entry *dir = search_dir_entry_in_list(path);

        while((dir->start_subdir) != NULL) {
                struct dir_entry *curr_subdir = dir->start_subdir;

                while(curr_subdir->next != NULL)
                        curr_subdir = curr_subdir->next;

                char* subdir_path = get_full_dir_path(curr_subdir);
                remove_dir_to_monitor(subdir_path);
        }

        while((dir->start_file) != NULL) {
                struct file_entry *curr_file = dir->start_file;
                while(curr_file->next != NULL)
                        curr_file = curr_file->next;

                char* loc = (char*) curr_file->loc;
                struct status_entry *status = create_status_entry(loc,
                                                                'F', 'D', 0);

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                remove_file_entry_from_list(curr_file);
                remove_file_entry_in_dir_entry(dir, curr_file);
        }

        char* loc  = (char*) dir->loc;
        struct status_entry *status = create_status_entry(loc, 'D', 'D', 0);

        pthread_mutex_lock(&mutex_changes);
        add_status_entry_to_list(&start_changes_list, status);
        pthread_mutex_unlock(&mutex_changes);

        struct watch_entry *watch = search_watch_in_list(start_watch_list, path);
        remove_watch_entry_from_list(&start_watch_list, watch);

        remove_dir_entry_from_list(dir);
        remove_dir_entry_in_parent_dir_entry(&dir);
}

int event_filter(const struct inotify_event *event)
{
        int n = 0;
        if ((event->mask) & IN_IGNORED)
                n = 1;

        if (!((event->mask) & IN_ISDIR) && ((event->mask) & IN_CREATE ))
                n = 1;

        if (strchr((event->name), '.') == (event->name))
                n = 1;

        if (strchr((event->name), '~') == ((event->name) +
                                strlen(event->name) - 1))
                n = 1;

        if (strcmp((event->name), "4913") == 0)
                n = 1;

        return n;
}

char *get_full_event_path(struct inotify_event *event)
{
        struct dir_entry *parent = search_watch_of_dir_entry_in_list(event->wd);
        char* parent_path = get_full_dir_path(parent);

        char* event_path = calloc((strlen(parent_path) + 1 + (event->len)),
                                                             sizeof(char));
        strcpy(event_path, parent_path);
        strcat(event_path, "/");
        strcat(event_path, (event->name));

        free(parent_path);

        return event_path;
}

char *get_rel_event_path(struct inotify_event *event)
{
        char *path = get_full_event_path(event);
        char *mdir = calloc((strlen("/home/") + strlen(user) + 2 ),
                                                      sizeof(char));
        strcpy(mdir, "/home/");
        strcat(mdir, user);
        strcat(mdir, "/");

        char *loc = calloc((strlen(path) - strlen(mdir) + 1), sizeof(char));
        strcpy(loc, path + strlen(mdir));

        free(path);
        free(mdir);

        return loc;
}

struct status_entry *process_event_IN_CREATE(struct inotify_event *event)
{
        char* path = get_full_event_path(event);
        char* loc = get_rel_event_path(event);

        struct status_entry *status = NULL;

        if ((event->mask) & IN_ISDIR) {
                struct dir_entry *dir = search_dir_entry_in_list(path);
                if(dir == NULL ) {
                        status = create_status_entry(loc, 'D', 'C',
                                                        (event->cookie));

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        struct watch_entry *watch = search_watch_in_list(
                                                  start_watch_list, path);
                        struct dir_entry *new_dir = create_dir_entry(path,
                                                                watch->wd);
                        add_dir_entry_to_list(new_dir);
                        add_dir_entry_to_parent_dir_entry(new_dir);
                } else {
                        struct watch_entry *del = search_watch_in_watch_list(
                                                start_watch_list, dir->watch);
                        remove_watch_entry_from_list(&start_watch_list, del);
                }
        }
        free(loc);
        free(path);
        return status;
}

struct status_entry *process_event_IN_DELETE(struct inotify_event *event)
{
        char* path = get_full_event_path(event);
        char* loc = get_rel_event_path(event);


        if ((event->mask) & IN_ISDIR) {
                struct status_entry *status = create_status_entry(loc, 'D',
                                                        'D', (event->cookie));

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                struct watch_entry *watch = search_watch_in_list(
                                           start_watch_list, path);
                remove_watch_entry_from_list(&start_watch_list, watch);

                struct dir_entry *del = search_dir_entry_in_list(path);
                remove_dir_entry_from_list(del);
                remove_dir_entry_in_parent_dir_entry(&del);

                free(loc);
                free(path);
                return status;
        } else {
                struct file_entry *del = search_file_entry_in_list(path);
                if(del != NULL) {
                        struct status_entry *status = create_status_entry(
                                                loc, 'F', 'D', (event->cookie));

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        remove_file_entry_from_list(del);

                        struct dir_entry *parent =
                                   search_watch_of_dir_entry_in_list(event->wd);

                        remove_file_entry_in_dir_entry(parent, del);

                        free(loc);
                        free(path);
                        return status;
                } else {
                        sd_journal_print(LOG_NOTICE, "Couldn't delete file: \
                                             %s not in list!", (event->name));
                        free(loc);
                        free(path);
                        return NULL;
                }
        }
}

struct status_entry *process_event_IN_CLOSE_WRITE(struct inotify_event *event)
{
        char* path = get_full_event_path(event);
        char* loc = get_rel_event_path(event);

        if(!((event->mask) & IN_ISDIR)) {
                struct file_entry *file = search_file_entry_in_list(path);
                if(file == NULL) {
                        struct status_entry *status = create_status_entry(
                                            loc, 'F', 'C', (event -> cookie) );

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        file = create_file_entry(path);
                        add_file_entry_to_list(file);

                        struct dir_entry *parent =
                                search_watch_of_dir_entry_in_list(event->wd);
                        add_file_entry_to_dir_entry(parent, file);

                        free(loc);
                        free(path);
                        return status;
                } else {
                        struct status_entry* status = create_status_entry(
                                                loc, 'F', 'W', (event->cookie));

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        free(loc);
                        free(path);
                        return status;
                }
        }
}

struct status_entry* process_event_IN_MOVED_FROM(struct inotify_event *event)
{
        char* path = get_full_event_path(event);
        char* loc = get_rel_event_path(event);

        if ((event -> mask) & IN_ISDIR) {
                struct status_entry *status = create_status_entry(loc, 'D',
                                                          'F', (event->cookie));

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                remove_dir_to_monitor(path);

                free(loc);
                free(path);
                return status;
        } else {
                struct file_entry *del = search_file_entry_in_list(path);
                if(del != NULL) {
                        struct status_entry *status = create_status_entry(
                                                loc, 'F', 'F', (event->cookie));

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        struct file_entry* del = search_file_entry_in_list(path);
                        remove_file_entry_from_list(del);

                        struct dir_entry* parent =
                                 search_watch_of_dir_entry_in_list(event -> wd);
                        remove_file_entry_in_dir_entry(parent, del);

                        status = create_status_entry(loc, 'F', 'D',
                                                        (event->cookie));

                        pthread_mutex_lock(&mutex_changes);
                        add_status_entry_to_list(&start_changes_list, status);
                        pthread_mutex_unlock(&mutex_changes);

                        free(loc);
                        free(path);
                        return status;
                } else {
                        sd_journal_print(LOG_NOTICE, "Couldn't delete file: %s \
                                                  not in list!", (event->name));
                        free(loc);
                        free(path);
                        return NULL;
                }
        }
}

struct status_entry* process_event_IN_MOVED_TO(struct inotify_event* event)
{
        char* path = get_full_event_path(event);
        char* loc = get_rel_event_path(event);

        if ((event->mask) & IN_ISDIR) {
                analyse_dir_to_monitor(path);

                struct status_entry* status = create_status_entry(loc, 'D',
                                                        'T', (event->cookie));

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                free(loc);
                free(path);
                return status;
        } else {
                struct file_entry* file = create_file_entry(path);
                add_file_entry_to_list(file);

                struct dir_entry* parent =
                                search_watch_of_dir_entry_in_list(event->wd);
                add_file_entry_to_dir_entry(parent, file);

                struct status_entry* status = create_status_entry(loc, 'F',
                                                                   'C', 0 );

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                status =
                create_status_entry(loc, 'F', 'T', (event -> cookie) );

                pthread_mutex_lock(&mutex_changes);
                add_status_entry_to_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                free(loc);
                free(path);
                return status;
        }
}

struct status_entry* process_event(struct inotify_event* event)
{
        struct status_entry* status = NULL;

        if ((event->mask) & IN_CREATE)
                status = process_event_IN_CREATE(event);

        if ((event->mask) & IN_DELETE)
                status = process_event_IN_DELETE(event);

        if ((event->mask) & IN_CLOSE_WRITE)
                status = process_event_IN_CLOSE_WRITE(event);

        if ((event->mask) & IN_MOVED_FROM)
                status = process_event_IN_MOVED_FROM(event);

        if ((event->mask) & IN_MOVED_TO)
                status = process_event_IN_MOVED_TO(event);

        return status;
}

struct status_entry* search_status_entry_IN_MOVED(struct status_entry* entry)
{
        struct status_list_entry* curr_list = start_changes_list;
        while(curr_list != NULL) {
                struct status_entry* status = curr_list->status;
                if(compare_status_entry(status, entry) == 101)
                        return status;

                curr_list = curr_list->next;
        }
        return NULL;
}

void remove_status_entries_between_events_of_IN_MOVED(void)
{
        struct status_list_entry* curr_list = start_changes_list;
        while(curr_list != NULL) {
                struct status_entry* status = curr_list->status;
                struct status_entry* rel_status = NULL;
                if(status->stat == 'F')
                        rel_status = search_status_entry_IN_MOVED(status);

                if (rel_status != NULL) {
                        struct status_list_entry* next_list = curr_list->next;
                        struct status_entry* next_status = next_list->status;

                        while(compare_status_entry(rel_status, next_status)
                                                                   < 1110 ) {
                        pthread_mutex_lock(&mutex_changes);
                        remove_status_entry_from_list(&start_changes_list,
                                                                next_status);
                        pthread_mutex_unlock(&mutex_changes);

                        next_list = curr_list->next;
                        next_status = next_list->status;
                        }
                }
                curr_list = curr_list->next;
        }
}

void remove_single_events_of_IN_MOVED(void)
{
        struct status_list_entry* curr_list = start_changes_list;
        while(curr_list != NULL) {
                struct status_entry* status = curr_list->status;
                struct status_entry* rel_status = NULL;
                if((status->stat == 'F') || (status->stat == 'T')) {
                        rel_status = search_status_entry_IN_MOVED(status);

                        if (rel_status == NULL) {
                                pthread_mutex_lock(&mutex_changes);
                                remove_status_entry_from_list(
                                                &start_changes_list, status);
                                pthread_mutex_unlock(&mutex_changes);
                        }
                }
                curr_list = curr_list->next;
        }
}

void analyse_changes_list(void)
{
        remove_status_entries_between_events_of_IN_MOVED();
        remove_single_events_of_IN_MOVED();
}

void confirm_changes(void)
{
        struct status_list_entry* curr_list = start_changes_list;
        while(curr_list != NULL) {
                struct status_entry* status = curr_list->status;
                curr_list = curr_list->next;

                struct status_entry* search_status =
                search_status_entry_in_list(start_updates_list, status, 1110);

                if(search_status == NULL )
                        continue;

                // char* current_entry_name = status -> entry_name_;
                // char  current_entry_stat = status -> entry_stat_;
                //
                // if( current_entry_stat == 'C' )
                // {
                //   printf(KBLU "Confirm creation of %s\n" RESET, current_entry_name);
                // }
                // if( current_entry_stat == 'D' )
                // {
                //   printf(KBLU "Confirm removal of %s\n" RESET, current_entry_name);
                // }
                // if( current_entry_stat == 'W' )
                // {
                //   printf(KBLU "Confirm write of %s\n" RESET, current_entry_name);
                // }
                // if( current_entry_stat == 'F' )
                // {
                //   printf(KBLU"Confirm IN_MOVED_FROM of %s\n" RESET, current_entry_name);
                // }
                // if( current_entry_stat == 'T' )
                // {
                //   printf(KBLU"Confirm IN_MOVED_TO of %s\n" RESET, current_entry_name);
                // }


                pthread_mutex_lock(&mutex_changes);
                remove_status_entry_from_list(&start_changes_list, status);
                pthread_mutex_unlock(&mutex_changes);

                remove_status_entry_from_list(&start_updates_list, search_status);
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

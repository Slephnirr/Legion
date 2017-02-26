/*
 * Author: Kevin Pack
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <pthread.h>
#include <systemd/sd-journal.h>

#include "event_entry.h"
#include "watch_entry.h"
#include "entry_control.h"
#include "status_entry.h"
#include "process_events.h"


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


void* process_events_thread(void* arg)
{
        printf("started process_events_thread...\n");
        struct event_list_entry *curr_list;
        struct inotify_event *curr_event;
        while(1) {
                pthread_mutex_lock(&mutex_PE_thread);
                pthread_cond_wait(&cond_mutex_2, &mutex_PE_thread);
                pthread_mutex_unlock(&mutex_PE_thread);

                // sleep(1);

                curr_list = start_event_list;
                while(curr_list != NULL && curr_list->next != NULL) {
                        printf("%s\n", (curr_list->event)->name);

                        printf("case1\n");
                        curr_event = curr_list->event;
                        if(curr_list->proc == 'n')
                               process_event(curr_event);
                        curr_list->proc = 'y';

                        curr_list = curr_list->next;
                        remove_event_from_list(&start_event_list,
                                                        curr_event);
                        // if() {
                        //
                        // } else {
                        //
                        // }

                }

                printf("case2\n");
                curr_event = curr_list->event;
                if(curr_list->proc == 'n')
                       process_event(curr_event);
                curr_list->proc = 'y';

                // exit(EXIT_SUCCESS);
                printf("analyse_changes_list...\n");
                analyse_changes_list();
                printf("confirm_changes...\n");
                confirm_changes();
                printf("done...\n");
                if(start_event_list->next == NULL) {
                        printf("sending...\n");
                        pthread_cond_signal(&cond_mutex_3);
                }
                printf("done2...\n");
        }
}

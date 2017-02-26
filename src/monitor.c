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
#include "status_entry.h"
#include "entry_control.h"

#include "monitor.h"


int event_filter(const struct inotify_event *event)
{
        int n = 0;
        if ((event->mask) & IN_IGNORED)
                n = 1;

        if (!((event->mask) & IN_ISDIR) && ((event->mask) & IN_CREATE))
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

void add_watch_for_new_dir(const struct inotify_event *event)
{
        struct watch_entry *watch = search_watch_in_watch_list(start_watch_list,
                                                                     event->wd);
        char* parent_loc = watch->loc;
        char* dir_path = calloc(strlen(parent_loc) + (event->len) + 1,
                                                        sizeof(char));
        strcpy(dir_path, parent_loc);
        strcat(dir_path, "/");
        strcat(dir_path, event->name);

        int mode = IN_CREATE | IN_DELETE | IN_CLOSE_WRITE | IN_MOVE;
        int w = inotify_add_watch(inotify_instance, dir_path, mode);

        struct watch_entry *new_watch = create_watch_entry(dir_path, w);

        pthread_mutex_lock(&mutex_watch);
        add_watch_entry_to_list(&start_watch_list, new_watch);
        pthread_mutex_unlock(&mutex_watch);

        free(dir_path);
}

void* monitor_file_system_thread(void* arg)
{
        // printf("started monitor_file_system_thread...\n");
        analyse_dir_to_monitor("/home/kevin/Projekte/Legion/TestEnv");
        // start_changes_list = NULL;

        // buf[4096] <=> 256 events
        char buf[4096]
                __attribute__ ((aligned(__alignof__(struct inotify_event))));
        const struct inotify_event *event;
        ssize_t len;
        char *ptr;
        int changes = 0;
        while(1) {
                len = read(inotify_instance, buf, sizeof(buf));
                if (len <= 0) {
                        sd_journal_print(LOG_ERR, "couldn't read event!\n");
                        exit(EXIT_FAILURE);
                }

                for (ptr = buf; ptr < buf + len;
                        ptr += sizeof(struct inotify_event) + event->len) {

                        event = (const struct inotify_event *) ptr;
                        if (event_filter(event) == 1)
                                continue;

                        if((event->mask & IN_ISDIR) &&
                                                (event->mask & IN_CREATE)) {
                                add_watch_for_new_dir(event);
                        }

                        add_event_to_list(&curr_event_list, event);
                        changes = 1;
                }

                if((changes != 0)) {
                        pthread_cond_signal(&cond_mutex_2);
                        changes = 0;
                }
        }
}

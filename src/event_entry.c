/*
 * Author: Kevin Pack
 */

#include <errno.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <systemd/sd-journal.h>

#include "event_entry.h"

void add_event_to_list(struct event_list_entry **Pstart_list,
                                const struct inotify_event *event)
{
        struct event_list_entry *new_list = calloc(1,
                                        sizeof(struct event_list_entry));
        new_list->event = calloc(1, (sizeof(struct inotify_event) +
                                                     (event->len)));
        new_list->event = memcpy((new_list->event), event, (sizeof(struct
                                                inotify_event) + (event->len)));

        struct event_list_entry *curr_list = *Pstart_list;
        if(curr_list == NULL) {
                *Pstart_list = new_list;
        } else {
                while((curr_list->next) != NULL)
                        curr_list = curr_list->next;

                curr_list->next = new_list;
        }
}

int compare_events(struct inotify_event *A, struct inotify_event *B)
{
        int mask_match = ((A->mask) == (B->mask));
        int name_match = (strcmp(A->name, B->name) == 0);
        int cookie_match = ((A->cookie) == (B->cookie));

        if (mask_match && name_match && cookie_match)
                return 1;
        else
                return 0;
}

void remove_event_from_list(struct event_list_entry **Pstart_list,
                                        struct inotify_event* event)
{
        struct event_list_entry *prev_list = NULL;
        struct event_list_entry *curr_list = *Pstart_list;
        struct inotify_event *curr_event = NULL;

        while(curr_list != NULL) {
                curr_event = curr_list->event;

                if(compare_events(curr_event, event) == 1) {
                        if(prev_list == NULL)
                                *Pstart_list = curr_list->next;
                        else
                                prev_list->next = curr_list->next;

                        free((struct inotify_event*) curr_event);
                        free(curr_list);
                        curr_list = NULL;
                } else {
                        prev_list = curr_list;
                        curr_list = curr_list->next;
                }
        }
}

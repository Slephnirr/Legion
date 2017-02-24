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
#include <dirent.h>

#include "watch_entry.h"



struct watch_entry *create_watch_entry(char* str, int wd)
{
        struct watch_entry *watch = calloc(1, sizeof(struct watch_entry));

        char *loc = calloc((strlen(str) + 1), sizeof(char));
        strcpy(loc, str);

        watch->wd = wd;
        watch->loc = loc;

        return watch;
}

void delete_watch_entry(struct watch_entry **Pwatch)
{
        struct watch_entry *watch = *Pwatch;
        free((watch->loc));
        watch->loc = NULL;
        free(watch);
        watch = NULL;
}

int compare_watch(struct watch_entry *A, struct watch_entry *B)
{
        int match = 0;
        if(((A->wd) == (B->wd)) && (strcmp((A->loc), (B->loc)) == 0))
                match = 1;
        return match;
}

struct watch_entry *search_watch_in_watch_list(struct watch_list_entry
                                                *start_list, int swatch)
{
        struct watch_entry *cwatch;
        struct watch_list_entry *curr_list = start_list;
        while(curr_list != NULL) {
                cwatch = curr_list->watch;

                if ((cwatch->wd) == swatch)
                        return cwatch;

                curr_list = curr_list->next;
        }
        return NULL;
}

struct watch_entry *search_watch_in_list(struct watch_list_entry *start_list,
                                                                  char * str)
{
        struct watch_entry *cwatch;
        struct watch_list_entry *curr_list = start_list;
        while(curr_list != NULL) {
                cwatch = curr_list->watch;

                if (strcmp(str, cwatch->loc) == 0)
                        return cwatch;

                curr_list = curr_list->next;
        }
        return NULL;
}


void add_watch_entry_to_list(struct watch_list_entry **Pstart_list,
                                         struct watch_entry *watch)
{
        struct watch_list_entry *new_list = calloc(1, sizeof(
                                    struct watch_list_entry));
        new_list->watch = watch;

        struct watch_list_entry *curr_list = *Pstart_list;
        if(curr_list == NULL) {
                *Pstart_list = new_list;
        } else {
                while((curr_list->next) != NULL) {
                        curr_list = curr_list->next;
                }
                curr_list->next = new_list;
        }
}


void remove_watch_entry_from_list(struct watch_list_entry **Pstart_list,
                                             struct watch_entry *watch)
{
        struct watch_list_entry *prev_list = NULL;
        struct watch_list_entry *curr_list = *Pstart_list;
        struct watch_entry* cwatch = NULL;

        while(curr_list != NULL) {
                cwatch = curr_list->watch;

                if(compare_watch(cwatch, watch) == 1) {
                        if(prev_list == NULL)
                                *Pstart_list = curr_list->next;
                        else
                                prev_list->next = curr_list->next;

                        delete_watch_entry(&cwatch);

                        free(curr_list);
                        curr_list = NULL;
                } else {
                        prev_list = curr_list;
                        curr_list = curr_list->next;
                }
        }
}

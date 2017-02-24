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


#include "entry_control.h"
#include "status_entry.h"


char *get_full_stat_path(struct status_entry *status)
{
        char* mdir = calloc((strlen("/home/") + strlen(user) + 2 ), sizeof(char));
        strcpy(mdir,"/home/");
        strcat(mdir,user);
        strcat(mdir,"/");
        const char *loc  = status->name;
        char* path = calloc((strlen(mdir) + strlen(loc) + 1), sizeof(char));
        strcpy(path, mdir);
        strcat(path, loc);
        free(mdir);
        mdir = NULL;
        return path;
}

struct status_entry *create_status_entry(char *name, char type, char stat,
                                                          uint32_t cookie)
{
        struct status_entry *status = calloc(1, sizeof(struct status_entry));

        char* loc = calloc((strlen(name) + 1), sizeof(char));
        strcpy(loc, name);

        status->name = loc;
        status->type = type;
        status->stat = stat;
        status->cookie = cookie;

        return status;
}

void delete_status_entry(struct status_entry **Pstatus)
{
        struct status_entry *status = *Pstatus;
        free((status->name));
        status->name = NULL;
        free(status);
        status = NULL;
}

int compare_status_entry(struct status_entry *A, struct status_entry *B)
{
        int n = 0;
        if(strcmp((A->name), (B->name)) == 0)
                n += 1000;

        if((A->type) == (B->type))
                n += 100;

        if((A->stat) == (B->stat))
                n += 10;

        if((A->cookie) == (B->cookie))
                n += 1;

        return n;
}

struct status_entry *search_status_entry_in_list(struct status_list_entry
                                  *start_list, struct status_entry *status,
                                                                  int acc)
{
        struct status_entry *curr_status;
        struct status_list_entry *curr_list = start_list;
        while(curr_list != NULL) {
                curr_status = curr_list->status;
                int match = compare_status_entry(curr_status, status);

                if (match >= acc)
                        return curr_status;

                curr_list = curr_list->next;
        }
        return NULL;
}

void add_status_entry_to_list(struct status_list_entry **Pstart_list,
                                         struct status_entry *status)
{
        struct status_list_entry *new_list = calloc(1,
                    sizeof(struct status_list_entry));
        new_list->status = status;

        struct status_list_entry *curr_list = *Pstart_list;
        if(curr_list == NULL) {
                *Pstart_list = new_list;
        } else {
                while((curr_list->next) != NULL)
                        curr_list = curr_list->next;

                curr_list->next = new_list;
        }
}

void remove_status_entry_from_list(struct status_list_entry **Pstart_list,
                                             struct status_entry *status)
{
        struct status_list_entry *prev_list = NULL;
        struct status_list_entry *curr_list = *Pstart_list;
        struct status_entry *curr_status = NULL;

        while(curr_list != NULL) {
                curr_status = curr_list->status;

                if(compare_status_entry(curr_status, status) >= 1110)
                {

                        if(prev_list == NULL)
                                *Pstart_list = curr_list->next;
                        else
                                prev_list->next = curr_list->next;

                        delete_status_entry(&status);

                        free(curr_list);
                        curr_list = NULL;
                } else {
                        prev_list = curr_list;
                        curr_list = curr_list->next;
                }
        }
}

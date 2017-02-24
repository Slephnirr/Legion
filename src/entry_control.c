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

#include <systemd/sd-journal.h>

//#include "watch_entry.h"
#include "entry_control.h"

int does_entry_exist_in_file_system(const char* str)
{
        int exist = 0;
        struct stat* attr = calloc(1, sizeof(struct stat));

        if(stat(str, attr) == 0)
                exist = 1;

        free(attr);
        return exist;
}

struct file_entry *create_file_entry(char *str)
{
        struct file_entry *file = calloc(1, sizeof(struct file_entry));

        char *hdir = calloc((strlen("/home/") + strlen(user) + 2 ), sizeof(char));
        strcpy(hdir, "/home/");
        strcat(hdir, user);
        strcat(hdir, "/");

        char *loc = calloc((strlen(str) - strlen(hdir) + 1), sizeof(char));
        strcpy(loc, str + strlen(hdir));

        file->loc = loc;
        return file;
}

char *get_full_file_path(struct file_entry *file)
{
        char* hdir = calloc((strlen("/home/") + strlen(user) + 2 ), sizeof(char));
        strcpy(hdir,"/home/");
        strcat(hdir,user);
        strcat(hdir,"/");

        const char *loc = file->loc;
        char *path = calloc((strlen(hdir) + strlen(loc) + 1), sizeof(char));
        strcpy(path, hdir);
        strcat(path, loc);

        free(hdir);
        hdir = NULL;
        return path;
}

void add_file_entry_to_dir_entry(struct dir_entry *dir, struct file_entry *file)
{
        if((dir->start_file) == NULL) {
                dir->start_file = file;
        } else {
                struct file_entry *file = dir->start_file;

                while((file->next) != NULL ) {
                        file = file->next;
                }

                file->next = file;
        }
}

void add_file_entry_to_list(struct file_entry *file)
{
        struct file_list_entry *curr = calloc(1, sizeof(struct file_list_entry));
        curr->file = file;
        if(start_file_list == NULL) {
                start_file_list = curr;
        } else {
                struct file_list_entry *list_entry = start_file_list;
                while((list_entry->next) != NULL) {
                        list_entry = list_entry->next;
                }

                list_entry->next = curr;
        }
}

struct file_entry *search_file_entry_in_list(char* str)
{
        char* path;
        struct file_list_entry *list_entry = start_file_list;
        while(list_entry != NULL) {
                path = get_full_file_path(list_entry->file);
                if (strcmp(path, str) == 0) {
                        free(path);
                        path = NULL;
                        return list_entry->file;
                }
                free(path);
                path = NULL;
                list_entry = list_entry->next;
        }
        path = NULL;
        return NULL;
}

void remove_file_entry_in_dir_entry(struct dir_entry *dir,
                                  struct file_entry *file)
{
        struct file_entry *prev = NULL;
        struct file_entry *curr = dir->start_file;
        while(curr != NULL) {
                if(curr == file)
                {
                        if(prev == NULL) {
                                dir->start_file = curr->next;
                                free((char*) curr->loc);
                                curr->loc = NULL;
                                free(curr);
                                curr = NULL;
                        } else {
                                prev->next = curr->next;
                                free((char*) curr->loc);
                                curr->loc = NULL;
                                free(curr);
                                curr = NULL;
                        }
                } else {
                        prev = curr;
                        curr = curr-> next;
                }
        }
}

void remove_file_entry_from_list(struct file_entry *file)
{
        struct file_list_entry *prev_list = NULL;
        struct file_list_entry *curr_list = start_file_list;
        struct file_entry *curr_file = NULL;
        while(curr_list != NULL) {
                curr_file = curr_list->file;
                if(curr_file == file) {
                        if(prev_list == NULL) {

                                if((curr_list->next) == NULL)
                                        start_file_list = NULL;
                                else
                                        start_file_list = curr_list->next;

                                free(curr_list);
                                curr_list = NULL;
                        } else {
                                prev_list->next = curr_list->next;
                                free(curr_list);
                                curr_list = NULL;
                        }
                } else {
                        prev_list = curr_list;
                        curr_list = curr_list->next;
                }
        }
}

char *get_full_dir_path(struct dir_entry *dir)
{
        char* hdir = calloc((strlen("/home/") + strlen(user) + 2 ),sizeof(char));
        strcpy(hdir, "/home/");
        strcat(hdir, user);
        strcat(hdir, "/");
        const char *loc = dir->loc;
        char *path = calloc((strlen(hdir) + strlen(loc) + 1), sizeof(char));
        strcpy(path, hdir);
        strcat(path, loc);
        free(hdir);
        hdir = NULL;
        return path;
}

char *get_parent_dir_path_of_dir_entry(struct dir_entry *dir)
{
        char* hdir = calloc((strlen("/home/") + strlen(user) + 2 ), sizeof(char));
        strcpy(hdir, "/home/");
        strcat(hdir, user);
        strcat(hdir, "/");
        const char *loc = dir->loc;
        char *path = calloc((strlen(hdir) + strlen(loc) + 1), sizeof(char));
        strcpy(path, hdir);
        strcat(path, loc);
        char *pos = strrchr(path, '/');
        *pos = '\0';
        free(hdir);
        hdir = NULL;
        return path;
}

struct dir_entry *create_dir_entry(char *str, int watch)
{
        struct dir_entry *dir = calloc(1, sizeof(struct dir_entry));
        char *hdir = calloc((strlen("/home/") + strlen(user) + 2 ), sizeof(char));
        strcpy(hdir, "/home/");
        strcat(hdir, user);
        strcat(hdir, "/");
        char *loc = calloc((strlen(str) - strlen(hdir) + 1), sizeof(char));
        strcpy(loc, str + strlen(hdir));

        dir->loc = loc;
        dir->watch = watch;

        return dir;
}

int entry_type(char *str)
{
        int n;  // 2 for Directories; 1 for Files; 0 unkown error
        struct stat *attr = calloc(1, sizeof(struct stat));
        if(stat((str), attr) == 0) {
                // Check if the data entry is a file
                if(S_ISDIR(attr->st_mode) == 1) {
                        n = 2;
                } else if(S_ISREG(attr->st_mode) == 1) {
                        n = 1;
                } else {
                        n = 0;
                }
        } else {
                sd_journal_print(LOG_ERR, "Couldn't read data\n");
                exit(EXIT_FAILURE);
        }
        free(attr);
        return n;
}

struct dir_entry *search_dir_entry_in_list(char *str)
{
        char *path;
        struct dir_list_entry *curr_list = start_dir_list;
        while(curr_list != NULL) {
                path = get_full_dir_path(curr_list->dir);
                if(strcmp(path, str) == 0) {
                        free(path);
                        path = NULL;
                        return curr_list->dir;
                }
                free(path);
                path = NULL;
                curr_list = curr_list->next;
        }
        path = NULL;
        return NULL;
}

struct dir_entry *search_watch_of_dir_entry_in_list(int swatch)
{
        int cwatch;
        struct dir_list_entry *curr_list = start_dir_list;
        while(curr_list != NULL) {
                cwatch = (curr_list->dir)->watch;

                if(cwatch == swatch)
                        return curr_list->dir;

                curr_list = curr_list->next;
        }
        return NULL;
}

int search_for_entry_in_dir_entry(struct dir_entry *dir, char *str)
{
        int exist = 0;
        char *path;
        struct dir_entry *subdir = dir->start_subdir;
        while(subdir != NULL) {
                path = get_full_dir_path(subdir);

                if(strcmp(path, str) == 0)
                        exist = 1;

                free(path);
                path = NULL;
                subdir = subdir->next;
        }
        struct file_entry *file = dir->start_file;
        while(file != NULL) {
                path = get_full_file_path(file);

                if(strcmp(path, str) == 0)
                        exist = 1;

                free(path);
                path = NULL;
                file = file->next;
        }
        return exist;
}

void add_dir_entry_to_parent_dir_entry(struct dir_entry *dir)
{
        char *path = get_parent_dir_path_of_dir_entry(dir);
        struct dir_entry *parent = search_dir_entry_in_list(path);
        if(parent != NULL) {
                if((parent->start_subdir) == NULL) {
                        parent->start_subdir = dir;
                } else {
                        struct dir_entry *subdir = parent->start_subdir;

                        while((subdir->next) != NULL)
                                subdir = subdir->next;

                        subdir->next = dir;
                }
        }
        free(path);
}

void add_dir_entry_to_list(struct dir_entry *dir)
{
        struct dir_list_entry *new_list = calloc(1,
                                                sizeof(struct dir_list_entry));
        new_list->dir = dir;

        if(start_dir_list == NULL) {
                start_dir_list = new_list;
        } else {
                struct dir_list_entry *curr_list = start_dir_list;

                while((curr_list->next) != NULL)
                        curr_list = curr_list->next;

                curr_list->next = new_list;
        }
}

int does_dir_entry_exist_in_list(char *str)
{
        int exist = 0;
        char *path;
        struct dir_list_entry *curr_list = start_dir_list;
        while(curr_list != NULL) {
        	path = get_full_dir_path(curr_list->dir);
        	if(strcmp(str, path) == 0) {
        		exist = 1;
        		break;
        	}
        	curr_list = curr_list->next;
        	free(path);
        }
        return exist;
}

void remove_dir_entry_in_parent_dir_entry(struct dir_entry** Pdir)
{
        struct dir_entry *dir= *Pdir;
        char *path = get_parent_dir_path_of_dir_entry(dir);
        struct dir_entry *parent = search_dir_entry_in_list(path);
        struct dir_entry *prev = NULL;
        struct dir_entry *subdir = parent->start_subdir;
        while(subdir != NULL) {
                if(strcmp((subdir->loc), (dir->loc)) == 0)
                {
                        if(prev == NULL)
                                parent->start_subdir = NULL;
                        else
                                prev->next = subdir->next;
                }
                prev = subdir;
                subdir = subdir->next;
        }

        free((char*) dir->loc);
        dir->loc = NULL;
        free(dir);
        dir = NULL;
        free(path);
}

void remove_dir_entry_from_list(struct dir_entry *dir)
{
        struct dir_list_entry *prev_list = NULL;
        struct dir_list_entry *curr_list = start_dir_list;
        struct dir_entry *curr_dir = NULL;
        while(curr_list != NULL) {
                curr_dir = curr_list->dir;
                if(curr_dir == dir) {
                        if(prev_list == NULL) {

                                if((curr_list->next) == NULL)
                                        start_dir_list = NULL;
                                else
                                        start_dir_list = curr_list->next;

                                free(curr_list);
                                curr_list = NULL;
                        } else {
                                prev_list->next = curr_list->next;
                                free(curr_list);
                                curr_list = NULL;
                        }
                } else {
                        prev_list = curr_list;
                        curr_list = curr_list->next;
                }
        }
}


// void update_file_properties(char *path)
// {
//         struct file_entry *file = search_file_entry_from_list(path);
//         struct stat* attr = calloc(1,sizeof(struct stat));
//         if (stat(path, attr) == 0) {
//                 // tlm = time_last_modified
//                 file->tlm = attr->st_mtime;
//                 file->size = attr->st_size;
//         } else {
//                 printf("Couldn't read file.\n");
//                 exit(EXIT_FAILURE);
//         }
//         free(attr);
// }

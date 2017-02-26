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
#include <pthread.h>

#include <systemd/sd-journal.h>

#include "status_entry.h"
#include "watch_entry.h"
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

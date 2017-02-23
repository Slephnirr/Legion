/*
 * Author: Kevin Pack
 */

const char* user;
int inotify_instance;

struct file_list_entry *start_file_list;
struct dir_list_entry *start_dir_list;
struct watch_list_entry *start_watch_list;


struct file_entry {
        const char *loc;
        struct file_entry *next;
};

struct file_list_entry {
        struct file_entry *file;
        struct file_list_entry *next;
};

struct dir_entry {
        int watch;
        const char *loc;
        struct dir_entry *next;
        struct file_entry *start_file;
        struct dir_entry *start_subdir;
};

struct dir_list_entry {
        struct dir_entry *dir;
        struct dir_list_entry *next;
};

int does_entry_exist_in_file_system(const char* str);

struct file_entry *create_file_entry(char* str);

char *get_full_file_path(struct file_entry *file);

void add_file_entry_to_dir_entry(struct dir_entry *dir, struct file_entry *file);

void add_file_entry_to_list(struct file_entry *file);

struct file_entry *search_file_entry_in_list(char* str);

void remove_file_entry_in_dir_entry(struct dir_entry *dir,
                                                struct file_entry *file);

void remove_file_entry_in_list(struct file_entry *file);

char *get_full_dir_path(struct dir_entry *dir);

char *get_parent_dir_path_of_dir_entry(struct dir_entry *dir);

struct dir_entry *create_dir_entry(char *str, int watch);

int entry_type(char *str);

struct dir_entry *search_dir_entry_in_list(char *str);

struct dir_entry *search_watch_of_dir_entry_in_list(int swatch);

int search_for_entry_in_dir_entry(struct dir_entry *dir, char *str);

void add_dir_entry_to_parent_dir_entry(struct dir_entry *dir);

void add_dir_entry_to_list(struct dir_entry *dir);

int does_dir_entry_exist_in_list(char *str);

void remove_dir_entry_in_parent_dir_entry(struct dir_entry** Pdir);

void remove_dir_entry_in_list(struct dir_entry *dir);

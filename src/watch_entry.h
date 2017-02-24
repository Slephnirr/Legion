/*
 * Author: Kevin Pack
 */


struct watch_entry{
        int wd;
        char* loc;
};

struct watch_list_entry{
	struct watch_entry *watch;
	struct watch_list_entry *next;
};

struct watch_entry *create_watch_entry(char* str, int wd);

void delete_watch_entry(struct watch_entry **Pwatch);

int compare_watch(struct watch_entry *A, struct watch_entry *B);

struct watch_entry *search_watch_in_watch_list(struct watch_list_entry
                                                *start_list, int swatch);

struct watch_entry *search_watch_in_list(struct watch_list_entry *start_list,
                                                                 char * str);

void add_watch_entry_to_list(struct watch_list_entry **Pstart_list,
                                          struct watch_entry *watch);

void remove_watch_entry_from_list(struct watch_list_entry **Pstart_list,
                                            struct watch_entry *watch);

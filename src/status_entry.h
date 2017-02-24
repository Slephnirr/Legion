/*
 * Author: Kevin Pack
 */


struct status_entry{
        char *name;
        char type;
        char stat;
        uint32_t cookie;
};

struct status_list_entry{
        struct status_entry *status;
        struct status_list_entry *next;
};

char *get_full_stat_path(struct status_entry *status);

struct status_entry *create_status_entry(char *name, char type, char stat,
                                                          uint32_t cookie);

void delete_status_entry(struct status_entry **Pstatus);

int compare_status_entry(struct status_entry *A, struct status_entry *B);

struct status_entry *search_status_entry_in_list(struct status_list_entry
                                  *start_list, struct status_entry *satus,
                                                                  int acc);

void add_status_entry_to_list(struct status_list_entry **Pstart_list,
                                        struct status_entry *status);

void remove_status_entry_in_list(struct status_list_entry **Pstart_list,
                                             struct status_entry *status);

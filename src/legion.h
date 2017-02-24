/*
 * Author: Kevin Pack
 */


struct event_list_entry *start_event_list;
struct status_list_entry *start_updates_list;
struct status_list_entry *start_changes_list;

pthread_mutex_t mutex_changes;

void list_all_file_entries(struct file_list_entry *start_list);

void list_all_dir_entries(struct dir_list_entry *start_list);

void list_status_entries(struct status_list_entry *start_list, char lswitch);

void list_events(struct event_list_entry *start_list);

void list_watch_entries(struct watch_list_entry *start_list);

void analyse_dir_to_monitor(char* path);

void remove_dir_to_monitor(char* path);

int event_filter(const struct inotify_event *event);

char *get_full_event_path(struct inotify_event *event);

char *get_rel_event_path(struct inotify_event *event);

struct status_entry *process_event_INCREATE(struct inotify_event *event);

struct status_entry *process_event_INDELETE(struct inotify_event *event);

struct status_entry *process_event_INCLOSEWRITE(struct inotify_event *event);

struct status_entry* process_event_INMOVEDFROM(struct inotify_event *event);

struct status_entry* process_event_INMOVEDTO(struct inotify_event* event);

struct status_entry* process_event(struct inotify_event* event);

struct status_entry* search_status_entry_INMOVED(struct status_entry* entry);

void remove_status_entries_between_events_of_INMOVED(void);

void remove_single_events_of_INMOVED(void);

void analyse_changes_list(void);

void confirm_changes(void);

int count_files_to_submit(void);

void provide_updates_for_node(int num, int sock);

int process_update_INCREATE(struct status_entry* status, int sock);

int process_update_INDELETE(struct status_entry* status, int sock);

int process_update_INCLOSEWRITE(struct status_entry* status, int sock);

int process_update_INMOVEDFROM(struct status_list_entry** Pcurr_list, struct
                                             status_entry* status, int sock);

int process_updates_on_node(int sock);

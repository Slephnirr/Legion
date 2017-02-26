/*
 * Author: Kevin Pack
 */


struct event_list_entry *start_event_list;
struct status_list_entry *start_updates_list;
struct status_list_entry *start_changes_list;


pthread_mutex_t mutex_SC_thread;
pthread_cond_t  cond_mutex_3;

pthread_mutex_t mutex_changes;

pthread_mutex_t mutex4;
pthread_cond_t  cond_mutex_4;

const char *device_ip;


void list_all_file_entries(struct file_list_entry *start_list);

void list_all_dir_entries(struct dir_list_entry *start_list);

void list_status_entries(struct status_list_entry *start_list, char lswitch);

void list_events(struct event_list_entry *start_list);

void list_watch_entries(struct watch_list_entry *start_list);

int count_files_to_submit(void);

void provide_updates_for_node(int num, int sock);

int process_update_IN_CREATE(struct status_entry* status, int sock);

int process_update_IN_DELETE(struct status_entry* status, int sock);

int process_update_IN_CLOSE_WRITE(struct status_entry* status, int sock);

int process_update_IN_MOVED_FROM(struct status_list_entry** Pcurr_list, struct
                                             status_entry* status, int sock);

int process_updates_on_node(int sock);

void *send_changes_thread(void* arg);

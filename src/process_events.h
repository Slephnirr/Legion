/*
 * Author: Kevin Pack
 */

pthread_mutex_t mutex_PE_thread;
pthread_cond_t cond_mutex_2;
pthread_cond_t cond_mutex_3;
pthread_mutex_t mutex_changes;


struct event_list_entry *curr_event_list;
struct status_list_entry *start_changes_list;
struct status_list_entry *start_updates_list;


struct status_entry *process_event_IN_CREATE(struct inotify_event *event);

struct status_entry *process_event_IN_DELETE(struct inotify_event *event);

struct status_entry *process_event_IN_CLOSE_WRITE(struct inotify_event *event);

struct status_entry* process_event_IN_MOVED_FROM(struct inotify_event *event);

struct status_entry* process_event_IN_MOVED_TO(struct inotify_event* event);

struct status_entry* process_event(struct inotify_event* event);

struct status_entry* search_status_entry_IN_MOVED(struct status_entry* entry);

void remove_status_entries_between_events_of_IN_MOVED(void);

void remove_single_events_of_IN_MOVED(void);

void analyse_changes_list(void);

void confirm_changes(void);

void* process_events_thread(void* arg);

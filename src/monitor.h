/*
 * Author: Kevin Pack
 */


pthread_mutex_t mutex_watch;
pthread_mutex_t mutex_event;
pthread_cond_t cond_mutex_2;
pthread_mutex_t mutex_changes;

struct event_list_entry  *curr_event_list;
struct status_list_entry *start_changes_list;


int event_filter(const struct inotify_event *event);

void add_watch_for_new_dir(const struct inotify_event *event);

void* monitor_file_system_thread(void* arg);

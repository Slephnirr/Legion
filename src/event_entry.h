/*
 * Author: Kevin Pack
 */

struct event_list_entry{
	char proc;
	struct inotify_event *event;
	struct event_list_entry *next;
};

struct event_list_entry *start_event_list;

char *get_full_event_path(struct inotify_event *event);

char *get_rel_event_path(struct inotify_event *event);

void add_event_to_list(struct event_list_entry **Pstart_list,
  		          const struct inotify_event *event);

int compare_events(struct inotify_event *A, struct inotify_event *B);

void remove_event_from_list(struct event_list_entry **Pstart_list,
                                     struct inotify_event* event);

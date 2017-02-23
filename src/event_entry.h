/*
 * Author: Kevin Pack
 */

struct event_list_entry{
	struct inotify_event *event;
	struct event_list_entry *next;
};

// struct event_list_entry *start_event_list;

void add_event_to_list(struct event_list_entry **Pstart_list,
  		          const struct inotify_event *event);

int compare_events(struct inotify_event *A, struct inotify_event *B);

void remove_event_from_list(struct event_list_entry **Pstart_list,
                                     struct inotify_event* event);

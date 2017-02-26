/*
 * Author: Kevin Pack
 */
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/inotify.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <fcntl.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <pthread.h>

#include <systemd/sd-journal.h>

#include "event_entry.h"
#include "watch_entry.h"
#include "entry_control.h"
#include "status_entry.h"
#include "network.h"
#include "monitor.h"
#include "process_events.h"
#include "send_changes.h"

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"
#define RESET "\033[0m"


// mutex_event:     used to lock the start_event_list while it is modified
// mutex_PE_thread: used to notify the ProcessUpdatesThread on new events
// mutex_changes:   used to lock the start_changes_list while it is modified
// mutex 4:         used to lock server from receiving when transmission is
//                  active and vice versa
pthread_mutex_t mutex_watch     = PTHREAD_MUTEX_INITIALIZER;
// pthread_mutex_t mutex_event     = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex_PE_thread = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_mutex_2    = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_SC_thread = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_mutex_3    = PTHREAD_COND_INITIALIZER;

pthread_mutex_t mutex_changes   = PTHREAD_MUTEX_INITIALIZER;

pthread_mutex_t mutex4          = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t  cond_mutex_4    = PTHREAD_COND_INITIALIZER;

const char *user = "kevin";
const char *device_ip;

int inotify_instance;


struct event_list_entry  *start_event_list   = NULL;
struct event_list_entry  *curr_event_list    = NULL;


struct file_list_entry   *start_file_list    = NULL;
struct dir_list_entry    *start_dir_list     = NULL;

struct watch_list_entry  *start_watch_list   = NULL;

struct status_list_entry *start_changes_list = NULL;
struct status_list_entry *start_updates_list = NULL;

void create_test_env(void)
{
        mode_t mode = S_IRWXU;
        mkdir("/home/kevin/Projekte/Legion/TestEnv", mode);
}

void create_test_env2(void)
{
        mode_t mode = S_IRWXU;
        mkdir("/home/kevin/Projekte/Legion/TestEnv",mode);
        mkdir("/home/kevin/Projekte/Legion/TestEnv/A",mode);
        // mkdir("/home/kevin/Projekte/Legion/TestEnv/B",mode);
        // mkdir("/home/kevin/Projekte/Legion/TestEnv/C",mode);
        mkdir("/home/kevin/Projekte/Legion/TestEnv/A/a1",mode);
        // mkdir("/home/kevin/Projekte/Legion/TestEnv/A/a2",mode);
        // mkdir("/home/kevin/Projekte/Legion/TestEnv/A/a3",mode);
        creat("/home/kevin/Projekte/Legion/TestEnv/A/test.txt", mode);
        creat("/home/kevin/Projekte/Legion/TestEnv/A/a1/test.txt", mode);
}







int main(void)
{
        printf("creating testenvironment...\n");
        create_test_env();

        // device_ip = "192.168.178.24";
        device_ip = "192.168.178.20";

        inotify_instance = inotify_init();
        if(inotify_instance == -1 ) {
                sd_journal_print(LOG_ERR, "Couldn't create inotify instance\n");
                exit(EXIT_FAILURE);
        }

        pthread_t sc_thread_id;
        if(pthread_create(&sc_thread_id,NULL,&send_changes_thread,
                                                &sc_thread_id) != 0) {
                sd_journal_print(LOG_ERR, "Couldn't create thread!\n");
                exit(EXIT_FAILURE);
        }

        pthread_t pe_thread_id;
        if(pthread_create(&pe_thread_id,NULL,&process_events_thread,
                                                &pe_thread_id) != 0) {
                sd_journal_print(LOG_ERR, "Couldn't create thread!\n");
                exit(EXIT_FAILURE);
        }

        pthread_t mfs_thread_id;
        if(pthread_create(&mfs_thread_id,NULL,&monitor_file_system_thread,
                                                     &mfs_thread_id) != 0) {
                sd_journal_print(LOG_ERR, "Couldn't create thread!\n");
                exit(EXIT_FAILURE);
        }

        int csock;
        int dsock;
        struct sockaddr_in* addr = calloc(1, sizeof(struct sockaddr_in));

        csock = start_listening(addr);

        while(1) {
                dsock = accept_comm(csock, addr);

                pthread_mutex_lock(&mutex4);

                clock_t start, end;
                double cpu_time_used;
                start = clock();

                start_updates_list = receive_status_list(dsock);

                list_status_entries(start_updates_list, 'A');

                send_int(1, dsock);

                int get_num = receive_int(dsock);

                int got_num = process_updates_on_node(dsock);

                close_socket(dsock);

                end = clock();
                cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;

                pthread_mutex_unlock(&mutex4);
        }

        close(inotify_instance);
        exit(EXIT_SUCCESS);
}

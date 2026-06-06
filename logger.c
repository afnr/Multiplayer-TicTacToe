#include <stdio.h>
#include <pthread.h>
#include "shared.h"

void *logger_thread_func(void *arg) {
    shared_data_t *data = (shared_data_t *)arg;

    // Open log file once
    FILE *fp = fopen("game.log", "a");
    if (!fp) {
        perror("game.log");
        return NULL;
    }

    while (1) {
        pthread_mutex_lock(&data->log_mutex);

        // Wait for log messages
        while (data->log_index == 0)
            pthread_cond_wait(&data->log_cond, &data->log_mutex);

        // Write all messages in the queue
        for (int i = 0; i < data->log_index; i++) {
            fprintf(fp, "%s\n", data->log_queue[i]);   // write to file
            fflush(fp);                                 // ensure immediate write

            printf("%s\n", data->log_queue[i]);        // optional: also print to terminal
        }

        // Reset log queue
        data->log_index = 0;

        pthread_mutex_unlock(&data->log_mutex);
    }

    fclose(fp);
    return NULL;
}

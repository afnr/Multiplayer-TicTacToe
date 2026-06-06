#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include <stdio.h>
#include "shared.h"

void load_scores(shared_data_t *data) {
    FILE *fp = fopen("scores.txt", "r");

    if (!fp) {
        fp = fopen("scores.txt", "w");
        for (int i = 0; i < MAX_PLAYERS; i++) {
            fprintf(fp, "0\n");
            data->scores[i] = 0;
        }
        fclose(fp);
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        fscanf(fp, "%d", &data->scores[i]);
    }

    fclose(fp);
}

void save_scores(shared_data_t *data) {
    pthread_mutex_lock(&data->score_mutex);

    FILE *fp = fopen("scores.txt", "w");
    if (!fp) {
        perror("scores.txt");
        pthread_mutex_unlock(&data->score_mutex);
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        fprintf(fp, "%d\n", data->scores[i]);
    }

    fclose(fp);
    pthread_mutex_unlock(&data->score_mutex);
}

#endif

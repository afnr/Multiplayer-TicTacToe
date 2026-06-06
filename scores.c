#include <stdio.h>
#include "shared.h"

void load_scores(shared_data_t *data) {
    FILE *f = fopen("scores.txt", "r");
    if (!f) {
        // If file not found, keep scores as 0
        return;
    }

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (fscanf(f, "%d", &data->scores[i]) != 1)
            data->scores[i] = 0;
    }

    fclose(f);
}
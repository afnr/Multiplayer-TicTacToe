#ifndef SHARED_H
#define SHARED_H

#include <pthread.h>

#define SHM_NAME "/tic_tac_toe_shm"

#define BOARD_SIZE 5
#define MAX_PLAYERS 3
#define EMPTY_CELL -1

#define LOG_QUEUE_SIZE 100
#define LOG_MSG_LEN 128

typedef struct {

    /* Game state */
    int board[BOARD_SIZE][BOARD_SIZE];
    int current_turn;
    int player_active[MAX_PLAYERS];
    int game_over;
    int winner;

    /* Persistent scores */
    int scores[MAX_PLAYERS];

    int terminate_system;

    /* Synchronization */
    pthread_mutex_t game_mutex;
    pthread_mutex_t turn_mutex;
    pthread_mutex_t score_mutex;

    /* Logger */
    pthread_mutex_t log_mutex;
    pthread_cond_t  log_cond;
    char log_queue[LOG_QUEUE_SIZE][LOG_MSG_LEN];
    int log_index;
    int all_players_ready;
    int move_made;

} shared_data_t;

/* Thread entry points (implemented by others) */
void *scheduler_thread_func(void *arg);
void *logger_thread_func(void *arg);

/* Persistence (implemented by others) */
void load_scores(shared_data_t *data);
void save_scores(shared_data_t *data);

#endif

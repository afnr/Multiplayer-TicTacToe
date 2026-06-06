#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>   // for mkfifo()
#include "shared.h"

/* FIFO paths */
static const char *fifo_in[MAX_PLAYERS] = {
    "/tmp/player1_in",
    "/tmp/player2_in",
    "/tmp/player3_in"
};

static const char *fifo_out[MAX_PLAYERS] = {
    "/tmp/player1_out",
    "/tmp/player2_out",
    "/tmp/player3_out"
};

shared_data_t *shared = NULL;

/* ---------- LOG EVENT (queue only) ---------- */
void log_event(const char *msg) {
    pthread_mutex_lock(&shared->log_mutex);

    if (shared->log_index < LOG_QUEUE_SIZE) {
        strncpy(shared->log_queue[shared->log_index], msg, LOG_MSG_LEN - 1);
        shared->log_queue[shared->log_index][LOG_MSG_LEN - 1] = '\0';
        shared->log_index++;
        pthread_cond_signal(&shared->log_cond);
    }

    pthread_mutex_unlock(&shared->log_mutex);
}

/* ---------- SIGCHLD HANDLER ---------- */
void handle_sigchld(int sig) {
    (void)sig;
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

/* ---------- INIT SHARED STATE ---------- */
void init_shared(shared_data_t *d) {
    for (int i = 0; i < BOARD_SIZE; i++)
        for (int j = 0; j < BOARD_SIZE; j++)
            d->board[i][j] = EMPTY_CELL;

    d->current_turn = 0;
    d->game_over = 0;
    d->winner = -1;
    d->log_index = 0;
    d->all_players_ready = 0;

    for (int i = 0; i < MAX_PLAYERS; i++) {
        d->player_active[i] = 0;
        d->scores[i] = 0;
    }
}

/*------------- LOAD SCORES ---------------*/
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

/*-------------- SAVE SCORES ----------------*/
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

    fflush(fp);
    fclose(fp);
    pthread_mutex_unlock(&data->score_mutex);
    printf("Scores saved successfully");
}

/*---------- PLAYERID TO SYMBOL ------------*/
char get_symbol(int player_id){
    switch(player_id){
        case 0:
            return 'X'; //player 1
        case 1:
            return '0'; //player 2
        case 2:
            return '#'; //player 3
    }
    return ' ';
}

/*------------ FORMAT BOARD ----------------*/
void format_board(shared_data_t *d, char *out, size_t size){
    int cell = 1; 
    char buf[64];
    out[0] = '\0';

    strncat(out, "+----+----+----+----+----+\n", size - strlen(out) - 1);
    for(int i = 0; i < BOARD_SIZE; i++){
        strncat(out, "|", size - strlen(out) - 1);
        for(int j = 0; j <BOARD_SIZE; j++){
            if (d->board[i][j] == EMPTY_CELL){
                    snprintf(buf, sizeof(buf), " %2d |", cell);
            }
            else{
                snprintf(buf, sizeof(buf), "  %c |", d->board[i][j]);
            }

            strncat(out, buf, size - strlen(out) - 1);
            cell++;
        }
        strncat(out, "\n+----+----+----+----+----+\n", size - strlen(out) - 1);
    }
}

/*---------------- CHECK WIN ------------------*/
int check_win(shared_data_t *d, int player_id){
    char symbol = get_symbol(player_id); //adjust later for the player symbol 

    //horizontal & vertical
    for(int i = 0; i < BOARD_SIZE; i++){
        for(int j = 0; j <= BOARD_SIZE - 3; j++){
            if(d->board[i][j] == symbol && d->board[i][j+1] == symbol && d->board[i][j+2] == symbol)
                return 1;
            if(d->board[j][i] == symbol && d->board[j+1][i] == symbol && d->board[j+2][i] == symbol)
                return 1;
        }
    }

    //diagonal 
    for(int i = 0; i <= BOARD_SIZE - 3; i++){
        for(int j = 0; j <= BOARD_SIZE - 3; j++){
            if(d->board[i][j] == symbol && d->board[i+1][j+1] == symbol && d->board[i+2][j+2] == symbol)
                return 1;
            if(d->board[i+2][j] == symbol && d->board[i+1][j+1] == symbol && d->board[i][j+2] == symbol)
                return 1;
        }
    }
    return 0;
}

/*-------------- CHECK DRAW ---------------*/   //draw is when there is no empty cell left in board and no winner 
int check_draw(shared_data_t *d){
    for(int i = 0; i < BOARD_SIZE; i++){
        for(int j =0; j < BOARD_SIZE; j++){
            if(d->board[i][j] == EMPTY_CELL)    //if there is still empty cell in the board
                return 0;
        }
    }
    return 1; //means no empty cell left
}

/*------------ VALIDATE MOVES ------------*/
int validate_moves(shared_data_t *d, int row, int col){
    //check bounds
    if(row < 0 || row >= BOARD_SIZE || col < 0 || col >= BOARD_SIZE){
        return 0; //out of range 
    }

    //check if the cell is empty 
    if(d->board[row][col] != EMPTY_CELL){
        return 0; //cell is already occupied
    } 

    return 1; //valid move
}

/* ---------- INIT MUTEXES & COND ---------- */
void init_sync(shared_data_t *d) {
    pthread_mutexattr_t mattr;
    pthread_condattr_t  cattr;

    pthread_mutexattr_init(&mattr);
    pthread_mutexattr_setpshared(&mattr, PTHREAD_PROCESS_SHARED);

    pthread_mutex_init(&d->game_mutex, &mattr);
    pthread_mutex_init(&d->turn_mutex, &mattr);
    pthread_mutex_init(&d->score_mutex, &mattr);
    pthread_mutex_init(&d->log_mutex, &mattr);

    pthread_mutexattr_destroy(&mattr);

    pthread_condattr_init(&cattr);
    pthread_condattr_setpshared(&cattr, PTHREAD_PROCESS_SHARED);
    pthread_cond_init(&d->log_cond, &cattr);
    pthread_condattr_destroy(&cattr);
}

/*------------ NUM TO COORDINATE ------------*/
void num_to_coords(int num, int *row, int *col){
    num--; // convert to 0-based indexing
    *row = num / BOARD_SIZE; //integer division
    *col = num % BOARD_SIZE; //remainder
}

/* ---------- CHILD PROCESS ---------- */
void client_handler(int player_id, int fd_in) {

    char buf[64];
    char msg[128];

    while (1) {
        //check if all players are ready 
        pthread_mutex_lock(&shared->game_mutex);
        int ready = shared->all_players_ready;
        int over = shared->game_over;
        pthread_mutex_unlock(&shared->game_mutex);

        if(!ready || over){
            usleep(200000); //wait until all players connected
            continue;
        }

        // Read move from FIFO (blocking) 
        int n = read(fd_in, buf, sizeof(buf) - 1);
        if (n <= 0) { //handle error
            continue;
        }
        buf[n] = '\0';

        // Parse move
        int num;
        if (sscanf(buf, "%d", &num) != 1){
            continue;
        }

            if (num == 0) {
                log_event("Player terminated the game\n");
                save_scores(shared);

                pthread_mutex_lock(&shared->game_mutex);
                shared->game_over = 1;
                shared->terminate_system = 1; //inform scheduler to terminate the system
                pthread_mutex_unlock(&shared->game_mutex);

                //broadcast termination to all players
                for (int i = 0; i < MAX_PLAYERS; i++) {
                    if (shared->player_active[i]) {
                        int out_fd = open(fifo_out[i], O_WRONLY | O_NONBLOCK);
                        if (out_fd >= 0) {
                            write(out_fd, "\n--- GAME TERMINATED BY PLAYER ---\n", 35);
                            close(out_fd);
                        }
                    }
                }
                sleep(1);
                //printf("Server shutting down..\n");
                kill(0, SIGTERM);
                exit(0); 
            }

        int r,c; 
        num_to_coords(num, &r, &c);

        // Update board if move is valid
        pthread_mutex_lock(&shared->game_mutex);

        if (!validate_moves(shared, r, c)) {
            // Send error back to this player only
            int out_fd = open(fifo_out[player_id], O_WRONLY);
            if (out_fd >= 0) {
                write(out_fd, "Invalid move! Cell out of range or already taken.\n",
                    strlen("Invalid move! Cell out of range or already taken.\n"));
                
                //resend prompt so client can try again
                write(out_fd, "Your turn (cell 1-25): ", strlen("Your turn (cell 1-25): "));
                close(out_fd);
            }
            pthread_mutex_unlock(&shared->game_mutex);
            continue;
        }

        //only valid moves reach here
        shared->board[r][c] = get_symbol(player_id);
        // Log the move
        snprintf(msg, sizeof(msg), "Player %d placed at cell %d",
                     player_id + 1, num);
        log_event(msg);

        //signal scheduler that a move happended
        shared->move_made = 1;

        //Send updated board to the clients
        char board_msg[512];
        format_board(shared, board_msg, sizeof(board_msg));

        for(int i = 0; i < MAX_PLAYERS; i++){
            if(shared->player_active[i]){
                int out_fd = open(fifo_out[i], O_WRONLY);
                if (out_fd >= 0){
                    //add new line between new board and last msg from server
                    write(out_fd, "\n", 1);
                    write(out_fd,board_msg, strlen(board_msg));
                    close(out_fd);
                }
            }
        }

        //Check win/draw
        if(check_win(shared, player_id)){
            shared->game_over = 1; 
            shared->winner = player_id;

            pthread_mutex_lock(&shared->score_mutex);
            shared->scores[player_id]++;
            pthread_mutex_unlock(&shared->score_mutex);

            snprintf(msg, sizeof(msg), "Game over: Player %d wins!",player_id + 1);
            log_event(msg);

            //broadcast the result to all players
            for(int i = 0; i < MAX_PLAYERS; i++){
                if(shared->player_active[i]){
                    int out_fd = open(fifo_out[i], O_WRONLY);
                    if (out_fd >= 0){
                        if (i == player_id) { 
                            char win_msg[50];
                            snprintf(win_msg, sizeof(win_msg), "You won !\n");
                            write(out_fd, win_msg, strlen(win_msg));
                        } 
                        else { 
                            char lose_msg[50];
                            snprintf(lose_msg, sizeof(lose_msg), "You lose ! Player %d won\n", player_id + 1);
                            write(out_fd, lose_msg, strlen(lose_msg));
                        }
                    }
                }
            }
            save_scores(shared);
        }

        else if(check_draw(shared)){
            shared->game_over = 1;
            shared->winner = -1;
            log_event("Game over: Draw!");

            //broadcast the result to all players
            for(int i = 0; i < MAX_PLAYERS; i++){
                if(shared->player_active[i]){
                    int out_fd = open(fifo_out[i], O_WRONLY);
                    if (out_fd >= 0){
                        write(out_fd, "Game ended in draw!\n", 21);
                        close(out_fd);
                    }
                }
            }
            save_scores(shared);
        }       
        pthread_mutex_unlock(&shared->game_mutex);
    }
}

/* ---------- ACCEPT CLIENTS ---------- */
void accept_clients(void) {

    //create fifos
    for (int i = 0; i < MAX_PLAYERS; i++) {
        unlink(fifo_in[i]);
        unlink(fifo_out[i]);
        mkfifo(fifo_in[i], 0666);
        mkfifo(fifo_out[i], 0666);
    }

    printf("Server waiting for clients...\n");

    while (1) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            pthread_mutex_lock(&shared->game_mutex);
            int active = shared->player_active[i];
            pthread_mutex_unlock(&shared->game_mutex);

            if (active) 
                continue; //already connected, skip 

            //server opens FIFO for reading moves
            int fd_in = open(fifo_in[i], O_RDWR);   
            int fd_out = open(fifo_out[i], O_WRONLY);
            if (fd_in < 0 || fd_out < 0) 
                continue;

            //mark player active in parent 
            pthread_mutex_lock(&shared->game_mutex);
            shared->player_active[i] = 1;
            char msg[128];
            snprintf(msg, sizeof(msg), "Player %d connected", i + 1);
            log_event(msg);
            pthread_mutex_unlock(&shared->game_mutex);

            pid_t pid = fork();
            if (pid == 0) {
                client_handler(i, fd_in); //child handles this player
            }
            close(fd_in);
            close(fd_out);
        }

        pthread_mutex_lock(&shared->game_mutex);

        //only check readiness if not already set 
        if(!shared->all_players_ready){
            int count = 0;
            for (int j = 0; j < MAX_PLAYERS; j++){
                if(shared->player_active[j])
                    count++;
            }

            if(count == MAX_PLAYERS){
                shared->all_players_ready = 1; 
                printf("All players have entered\n");

                //kick off Player 1's first turn
                shared->current_turn = 0;
                char board_msg[1024];
                format_board(shared, board_msg, sizeof(board_msg));

                int out_fd = open(fifo_out[0], O_WRONLY);
                if(out_fd >= 0){
                    //write(out_fd, "\n--- Game Started ---\n", 23);
                    write(out_fd, board_msg, strlen(board_msg));
                    write(out_fd, "Your turn (cell 1-25): ", strlen("Your turn (cell 1-25): "));
                    close(out_fd);
                }
                printf("Now is turn Player 1\n");
            }
        }
        pthread_mutex_unlock(&shared->game_mutex); 
        usleep(200000);
    }
}


//new game (multigame)
void reset_game(shared_data_t *d){
    pthread_mutex_lock(&d->game_mutex);

    //clear board
    for (int i = 0; i < BOARD_SIZE; i++){
        for (int j = 0; j < BOARD_SIZE; j++){
            d->board[i][j] = EMPTY_CELL;
        }
    }

    d->game_over = 0;
    d->winner = -1;
    d->move_made = 0;

    //continue w same players and + prev scores, no need to reset them
    pthread_mutex_lock(&d->turn_mutex);
    d->current_turn = 0;
    pthread_mutex_unlock(&d->turn_mutex);

    for (int i = 0; i < MAX_PLAYERS; i++){
        if (d->player_active[i]){
            int out_fd = open(fifo_out[i], O_WRONLY);
            if(out_fd >= 0){
                write(out_fd, "\n--- New Game Started ---\n", 26);
                if (i == 0 ){
                    write(out_fd, "Press 0 to terminate the game\n", 30);
                    char board_msg[512];
                    format_board(d, board_msg, sizeof(board_msg));
                    write(out_fd, board_msg, strlen(board_msg));
                    write(out_fd, "Your turn (cell 1-25): ", 23);
                } else {
                    write(out_fd, "Waiting for Player 1 to move\n", 29);
                }
                close(out_fd);
            }
        }
    }
    pthread_mutex_unlock(&d->game_mutex);
    log_event("New game session initialised");
}

/*--------------- SCHEDULER THREAD ---------------*/
void *scheduler_thread_func(void *arg) {
    shared_data_t *data = (shared_data_t *)arg;

    while(!data->all_players_ready){
        usleep(100000);
    }

    while (1) {
        // Use the game_mutex to check if the game is over
        pthread_mutex_lock(&data->game_mutex);
        int is_over = data->game_over;
        int move = data->move_made;
        pthread_mutex_unlock(&data->game_mutex);

        if(move && !is_over){
            // Lock turn_mutex to update current_turn in shared memory
            pthread_mutex_lock(&data->turn_mutex);
            int current = data->current_turn;

            // Round Robin: Check the next players to find an active one
            for (int i = 1; i <= MAX_PLAYERS; i++) {
                int candidate = (current + i) % MAX_PLAYERS;
                
                // Access the active status set by child processes
                if (data->player_active[candidate]) {
                    data->current_turn = candidate;

                    //Announce turn to the player 
                    for(int p = 0; p < MAX_PLAYERS; p++){
                        if(data->player_active[p]){
                            //int out_fd = open(fifo_out[p], O_WRONLY | O_NONBLOCK); // <- ediit sini
                            int out_fd = open(fifo_out[p], O_WRONLY); 
                            if (out_fd >= 0){
                                if (p == candidate){
                                    char *turn_msg = "Your turn\nSelect a cell to move (1-25): ";
                                    write(out_fd, turn_msg, strlen(turn_msg));
                                } else {
                                    //display wait message if it;s other player's turn
                                    char wait_msg[50];
                                    snprintf(wait_msg, sizeof(wait_msg), "Waiting for Player %d to move\n", candidate + 1);
                                    write(out_fd, wait_msg, strlen(wait_msg));
                                }
                                close(out_fd);
                            }
                        }
                    }
                    //debug log
                    printf("Now Player %d's turn\n", candidate + 1);
                    break;
                }
            }
            data->move_made = 0;
            pthread_mutex_unlock(&data->turn_mutex);
        }

        if (is_over) {
            sleep(5); //give time for players to know their prev game's result
            reset_game(data); //clear board for new game
            log_event("New round starting...");
            continue;
        }
    }
    return NULL;
}

/* ---------- MAIN ---------- */
int main(void) {
    struct sigaction sa = {0};
    sa.sa_handler = handle_sigchld;
    sigaction(SIGCHLD, &sa, NULL);

    shm_unlink(SHM_NAME);
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    ftruncate(shm_fd, sizeof(shared_data_t));

    shared = mmap(NULL, sizeof(shared_data_t),
                  PROT_READ | PROT_WRITE,
                  MAP_SHARED, shm_fd, 0);
    close(shm_fd);

    init_sync(shared);
    init_shared(shared);
    load_scores(shared);

    pthread_t sched, logt;
    pthread_create(&sched, NULL, scheduler_thread_func, shared);
    pthread_create(&logt, NULL, logger_thread_func, shared);

    accept_clients();
    return 0;
}

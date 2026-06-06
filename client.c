#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#define MAX_BUF 1024

//FIFO paths must match server.c
const char *fifo_in[] = {
    "/tmp/player1_in",
    "/tmp/player2_in",
    "/tmp/player3_in"
};

const char *fifo_out[] = {
    "/tmp/player1_out",
    "/tmp/player2_out",
    "/tmp/player3_out"
};

/*------------ PROMPT PLAYER ------------*/
void client_loop(int fd_out, int fd_in){
    char buf[MAX_BUF];

    while(1){
        //read update from server
        int n = read(fd_out, buf, sizeof(buf)-1);
        if(n <= 0)
            break;
        buf[n] = '\0';

        //show server message (could be board, turn prompt, win/draw)
        printf("%s", buf);
        fflush(stdout);

        //if it is the player's turn, prompt for a cell number
        if(strstr(buf, "Your turn")){
            int num;
            //printf("Enter cell number (1-25): ");

            if(scanf("%d", &num) != 1){
                fprintf(stderr, "Invalid input\n");
                //clear stdin buffer
                int c; 
                while((c = getchar()) != '\n' && c != EOF);
                continue; //retry loop
            }

            //send the num to the server
            snprintf(buf, sizeof(buf), "%d", num);
            write(fd_in, buf, strlen(buf));
        }

        //if server announces game over, wait for game to reset
        if(strstr(buf, "wins") || strstr(buf, "lose") || strstr(buf, "draw")){
            printf("\n--- Game finished! ---\n");
        }
    }
    close(fd_out);
    close(fd_in);
}

int main(int argc, char *argv[]) {

    if(argc != 2){
        fprintf(stderr, "Usage: %s <player_id>\n", argv[0]);
        return 1;
    }

    int player_id = atoi(argv[1]) - 1;
    if(player_id < 0 || player_id > 2){
        fprintf(stderr, "Invalid player id\n");
        return 1;
    }

    const char *in_fifo = fifo_in[player_id];
    const char *out_fifo = fifo_out[player_id];
    
    //open output FIFO for reading (server->client)
    int fd_out = open(out_fifo, O_RDONLY);
    if (fd_out < 0){
        perror("open out_fifo");
        return 1;
    }

    //open input FIFO for writing (client->server)
    int fd_in = open(in_fifo, O_WRONLY);
    if (fd_in < 0){
        perror("open in_fifo");
        return 1;
    }

    printf("Connected as Player %d\n", player_id + 1);

    client_loop(fd_out, fd_in);
    
    close(fd_out);
    close(fd_in);
    return 0;
}

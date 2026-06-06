Concurrent Multiplayer Tic-Tac-Toe (5x5 Variant)
For demo: https://youtu.be/aulFo3OEw-A
This file provides important information for compiling, running, and testing the project. It explains the commands required to build and execute the server and clients, summarizes the rules of the Tic-Tac-Toe game variant, and clarifies the supported deployment mode. The goal is to guide users and evaluators in quickly setting up and understanding the system.  

COMPILATION 
----------------------
make clean to clean up compiled files.
The project uses a Makefile for compilation. Use make to compile. Ensure you are on a Linux system with gcc and phtread installed.

EXECUTING PROGRAM 
----------------------------------
1. Start the server in one terminal:
./server
2. Start each client in separate terminal. Each client requires a player ID (1-3):
- ./client 1
- ./client 2
- ./client 3
3. The server waits until all players are connected, then begins the game.

EXAMPLE COMMANDS
--------------------------------
#Clean up compiled files
make clean

#Compile 
make 

#Run server
./server

#Run clients
./client 1
./client 2
./client 3

GAMES RULES SUMMARY
--------------------------------------
Board: 5x5 grid (cell numbered 1-25).
Players: 3 players (Player 1 = X, Player 2 = O, Player 3 = #).
Turn Order: Managed by Round Robin scheduler (fair cyclic turns).
Valid Move: Enter a cell number (1-25) that is empty. 
Win Condition: Align 3 consecutive symbols horizontally, vertically, or diagonally. 
Draw Condition: Board full with no winner. 
Termination: Enter 0 to terminate the game. 
Multi-Game: After win/draw, server resets board and starts a new round. Scores persist across games. 

MODE SUPPORTED 
----------------------------
Single-machine mode (IPC):
- Communication between server and clients via named pipes (FIFOs).
- Shared memory + process-shared mutexes handle game state, logging, and scores.
Note: TCP mode is not implemented; IPC was chosen for simplicity, reliability, and alignment with assignment requirements. 

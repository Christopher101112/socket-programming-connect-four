/*
 * Connect Four header
 * Contains function declarations, allowing connectfour.c to use those functions.
 * This header file should always be stored in the same directory as connectfour.c.
 * @author = ChristopherPouch
 * 2021
 */ 

//Game world setup
void initialization(char *port, char *IP);

//Gets next move from player, checks it is valid
int checkInput(char move); 

//Updates the game world based on the move.
int updateWorld(char selection); 

//After every move, checks whether the game has been won.
int checkWinner(int index);

//Prints the updated game board to screen.
void displayWorld(); 

//Called when the game is over
void teardown();

//Sets up the server, waits for client to request connection.
int server(char *port);

//Sets up client and connects to server if one exists (must be called after server() called).
int client(char *IP, char *port);

//Client makes moves and processes moves made by server.
int clientGameLoop();

//Server makes moves and processes moves made by client.
int serverGameLoop();

//Check whether a certain column is full.
int columnFull(char checkIt);

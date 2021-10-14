/*
 * Connect Four 2-person Game
 * One player acts as server, the other as client.
 * Players connect remotely via socket programming.
 * @author = ChristopherPouch
 * 2021
 */ 
#include <stdio.h>
#include"cfourheader.h"
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>  
#include <unistd.h> 

/*
 * To understand index choices throughout source code, 
 * see game board indexing system here. 
 * 
 *      A  B  C  D  E  F  G      (A=1...G=6)
 *      7  8  9  10 11 12 13
 *      14 15 16 17 18 19 20
 *      21 22 23 24 25 26 27
 *      28 29 30 31 32 33 34
 *      35 36 37 38 39 40 41
 *      42 43 44 45 46 47 48
 */  

/*
 * To compile and run multiplayer game:
 * Open two terminal windows, one will be server, one will be client.
 * 
 * First set up server
 * gcc connectfour.c
 * ./a.out portNumber
 * 
 * Then setup client
 * gcc connectfour.c
 * ./a.out 999.999.9.99 portNumber  //IP of server computer, ports for client and server must match.
 */

//Player names
char serverName[25];
char clientName[25];
char *pname; //Points to player whose turn it is.

char piece1 = 'X';
char piece2 = 'O';
char *ppiece; //Will switch between X an O based on whose turn it

//Game board 
int columns = 7;
int rows = 7;
int columnBase; //(index) Will change based on which column is selected
int columnTop;  
char *board;    //Used to update board in updateWorld()
int countMoves = 1; //Count the number of successful moves made so we know when board is full. 
char selection; //Column chosen by player
char *pselection = &selection; //Points to selection to change it from within input().

//Connections
//Socketdescriptor to be used in client comm. functions.
int socketDescriptor = -1;
int *psocketDescriptor = &socketDescriptor; //pointer to socketDescriptor.

//newfd to be used in server comm. fuctions.
int newfd= -1;
int *pnewfd = &newfd;

//If client quits, server() flow must exit serverSend().
//This condition is caught in serverReceive(), which needs a way to tellserverSend().
//So it uses this pointer.
int exitServerSend = 0;  //Changed to 1 if client terminates.
int *pexitServerSend = &exitServerSend;

//Passed into initialize() when no IP arg entered.
char IPArg[] = {'0'};

/*
 * Entry point for the program
 * Processes input arguments, sets of server or client processes accordingly.
 */
int main(int argc, char *argv[]){
    
    //char currentMove; //Stores input() char return.
    //int continueLoop = 1; //Only becomes 0 if game ends

	//Initialize() will call the server setup because no IP was passed.
	if( argc == 2){
		initialization(argv[1], IPArg);
	}
	//If IP and port (server info, client connects to) passed, initialize() will call client setup.
	else if(argc ==3){
		initialization(argv[1], argv[2]); 
	}
	//Otherwise there is a problem
	else{
		printf("\nTo run as server, pass port # as arg.\n");
		printf("To run as client, pass IP of server and same port # as server.\n");
		printf("\nIf port # below 1024, SUDO may be required.");
	}
    
    return 0;
}

/**
 * If user connects as client, their game loop will run here.
 */
int clientGameLoop(){

    int clientEvenOdd = 1;  //Keep track of whose turn it is.
    int continueLoop = 1; //Only becomes 0 if game ends
    char inputString[100];
    char move;
    
    while(continueLoop == 1){
        
        //If it is server's turn, get server's move.
        if(clientEvenOdd%2==1){
            pname = serverName;
            ppiece = &piece1;
            
            //Get server's move
            printf("Awaiting move from %s", pname);
            int received = recv(*psocketDescriptor, inputString, 100, 0);
            if(received==0){
                printf("Server disconnected, ending game.\n");
                teardown();
                break;
            }
            move = *inputString; //Get first character input (usually there should only be one).

        //If it is client's turn, get (and share) client's move.
        }else{
            pname = clientName;
            ppiece = &piece2;
            printf("\nYour move, %s", pname);
            
            
            //Get client's move, check it
            fgets(inputString, 100, stdin);
            move = *inputString;

            //If move is valid, proceed.
            if( checkInput(move)==0 && columnFull(move)==0 ){
                int sent = send(*psocketDescriptor, inputString, 100, 0);
            
            //If invalid column selected, get a valid one.
            }else if(checkInput(move)==1 || columnFull(move)==1){
                
                //Loop until we get a valid move.
                while(checkInput(move)==1 || columnFull(move)==1){
                    printf("Invalid column choice.\n");
                    
                    if(checkInput(move)==1){
                        printf("Please choose from column options {A, B, C, D, E, F, G} \n");
                    }else{
                        printf("Column %c is full, please choose another column.\n", move);
                    }

                    fgets(inputString, 100, stdin);
                    move = *inputString;
                }
                int sent = send(*psocketDescriptor, inputString, 100, 0); //Only reached after valid column choice.
            }
            
        }

        //Move is already verified as valid.
        continueLoop = updateWorld(move);

        //If valid move made, game continues (most common case).
        if(continueLoop==1){   
            //Don't enter teardown().
            displayWorld();
        
        //If player quit the game.              
        }else if(continueLoop==0){
            printf("\nThe game was quit by %s", pname);
            teardown();   
        
        //If the latest move won the game, crown winner and enter teardown().
        }else if(continueLoop==2){
            printf("\nThe game was won by %s", pname);
            displayWorld(); //Show the winning game board
            printf("\nCongratulations %s", pname);
            teardown();
        
        //If the board is completely full, the game ends in a draw.
        }else if(continueLoop==3){
            printf("\nThe board if full. This match ends in a draw.");
            teardown();
        
        //Control flow should never get to this case.
        }else{
            printf("Unknown error, game terminating...");
            teardown();
        }
        countMoves++;
        clientEvenOdd++;
    }
    return 0;
}

/**
 * If user connects as server, their game loop will run here.
 */
int serverGameLoop(){
    
    int serverEvenOdd = 1;  //Keep track of whose turn it is.
    int continueLoop = 1; //Only becomes 0 if game ends
    char inputString[100];
    char move;
    
    while(continueLoop == 1){
        
        //If it is server's turn
        if(serverEvenOdd%2==1){
            pname = serverName;
            ppiece = &piece1;
            
            printf("\nYour move, %s", pname); //Fgets() has trailing \n char. Don't print anything after the %s.        

            //Get server's move, send it to client
            //Set server name, send it over
            fgets(inputString, 100, stdin);
            move = *inputString;
            
            //Check move is valid, send if it is, get new move if not.
            
            //If player selects a valid column, proceed.
            if( checkInput(move)==0 && columnFull(move)==0 ){
                int sent = send(*pnewfd, inputString, 100, 0);
            
            //If invalid column selected, get a valid one.
            }else if(checkInput(move)==1 || columnFull(move)==1){
                
                //Loop until we get a valid move.
                while(checkInput(move)==1 || columnFull(move)==1){
                    printf("Invalid column choice.\n");
                    
                    if(checkInput(move)==1){
                        printf("Please choose from column options {A, B, C, D, E, F, G} \n");
                    }else{
                        printf("Column %c is full, please choose another column.\n", move);
                    }

                    fgets(inputString, 100, stdin);
                    move = *inputString;
                }
                int sent = send(*pnewfd, inputString, 100, 0); //Only reached once legal column selected.
            }

        //If it is client's turn
        }else{
            pname = clientName;
            ppiece = &piece2;

            //receive client's move.
            printf("Awaiting move from %s", pname);
            int received = recv(*pnewfd, inputString, 100, 0);
            if(received==0){
                printf("Client disconnected, ending game.");
                teardown();
                break;
            }
            move = *inputString; 
        }
      
        //The move has already been verified as valid.
        continueLoop = updateWorld(move);      

        //If valid move made, game continues (most common case).
        if(continueLoop==1){   
            //Don't enter teardown().
            displayWorld();
        
        //If player quit the game.              
        }else if(continueLoop==0){
            printf("\nThe game was quit by %s", pname);
            teardown();   
        
        //If the latest move won the game, crown winner and enter teardown().
        }else if(continueLoop==2){
            printf("\nThe game was won by %s", pname);
            displayWorld(); //Show the winning game board
            printf("\nCongratulations %s", pname);
            teardown();
        
        //If the board is completely full, the game ends in a draw.
        }else if(continueLoop==3){
            printf("\nThe board is full. This match ends in a draw.");
            teardown();
        
        //If the selected column is full, print message and restart the loop.
        }else if(continueLoop==4){
            printf("Column %c is full, please select a different column.", move);
            continue;

        //Control flow should never get to this case.
        }else{
            printf("Unknown error, game terminating...");
            teardown();
        }

        countMoves++; //The board can only become full after client's last turn.
        serverEvenOdd++;
    }
    return 0;
}

/*
 * Check if selected column is full. 
 * Return 0 for not full, 1 for full.
 */
int columnFull(char checkIt){

    switch (checkIt)
	{
		case 'A':
        case 'a':
			columnTop = 7;
			columnBase = 42;
			break;
		
		case 'B':
        case'b':
			columnTop = 8;
			columnBase = 43;
			break;

		case 'C':
        case 'c':
			columnTop = 9;
			columnBase = 44;
			break;

		case 'D':
        case 'd':
			columnTop = 10;
			columnBase = 45;
			break;

		case 'E':
        case 'e':
			columnTop = 11;
			columnBase = 46;
			break;

		case 'F':
        case 'f':
			columnTop = 12;
			columnBase = 47;
			break;

		case 'G':
        case 'g':
			columnTop = 13;
			columnBase = 48;
			break;
	}
    //If top spot already has a piece, column is full.
	if(*(board+columnTop) != '.'){
        return 1; //Full
    }
    return 0; //Not full
}

/*
 * Get player names, state game rules.
 * Dynamically allocate space for the game board.
 */
void initialization(char *port, char *IP){

    int serverSetup, clientSetup; //To tell whether setup was successful.

    //Based on input args call correct setup.
	if(*IP == '0'){
        serverSetup = server(port);
	}else{
        clientSetup = client(port, IP);
	}
    //Exit initialize() if either user fails to set up correctly.
	if(*IP == '0'){
        if(serverSetup!=0){
            return;
        }
	}else{
        if(clientSetup!=0){
            return;
        }
	}
    
    while(*serverName == '\0' && *clientName == '\0'){
        //Wait until both names filled out.
    }
    
    printf("\n\n");
    printf("Setting up the game...");
    printf("Ready to begin game:\n%svs\n%s", serverName, clientName);

    printf("\nOn each turn, select which column to drop your piece into. \nValid columns are A, B, C, D, E, F, and G. ");
    printf("(Type 'Q' to quit game).\n"); 

    //Create game board
	board = malloc(columns*rows*sizeof(char));  //Type cast not necessary, malloc's void * return is auto-upgraded to char *.

    //Check whether malloc worked
    if(board == NULL){
        printf("Memory allocation did not work.");
    }
    //Name the top of columns
	*board     ='A';
	*(board+1) ='B';
	*(board+2) ='C';
	*(board+3) ='D';
	*(board+4) ='E';
	*(board+5) ='F';
	*(board+6) ='G';

    //Empty positions at initialization filled with '.'
	for(int i=7; i<rows*columns; i++){	
		*(board + i) = '.';	
	}

    displayWorld();

    //Based on input args call correct game loop.
	if(*IP == '0'){
        if(serverSetup==0){
            serverGameLoop();
        }
	}else{
        if(clientSetup==0){
            clientGameLoop();
        }
	}
    //If flow reaches here it returns to main and program ends.
}

/*
 * Request and receive player column choice.
 * Input must be in {A,B,C,D,E,F,G,Q}.
 */
int checkInput(char move){
    
    *pselection = move;

    //Must check input column is valid, or else updateWorld() could fail to work.
    if(selection=='A'||selection=='B'||selection=='C'||selection=='D'||selection=='E'||selection=='F'||selection=='G'||selection=='Q'){
        //The input is valid
    
    //Input can be lower case or upper case.    
    }else if(selection=='a'||selection=='b'||selection=='c'||selection=='d'||selection=='e'||selection=='f'||selection=='g'||selection=='q'){
        //The input is valid
    
    //If input character is invalid, return 1.   
    }else{
        return 1;
    }
    return 0;
}

/*
 * The game board is updated based upon player input.
 * Return 0 if player quits the game
 * Return 1 for normal successful move.
 * Return 2 if a move won the game
 * Return 3 if the board is full and no more moves can be made.
 */
int updateWorld(char columnChar){

    switch (columnChar)
	{
		case 'A':
        case 'a':
			columnTop = 7;
			columnBase = 42;
			break;
		
		case 'B':
        case'b':
			columnTop = 8;
			columnBase = 43;
			break;

		case 'C':
        case 'c':
			columnTop = 9;
			columnBase = 44;
			break;

		case 'D':
        case 'd':
			columnTop = 10;
			columnBase = 45;
			break;

		case 'E':
        case 'e':
			columnTop = 11;
			columnBase = 46;
			break;

		case 'F':
        case 'f':
			columnTop = 12;
			columnBase = 47;
			break;

		case 'G':
        case 'g':
			columnTop = 13;
			columnBase = 48;
			break;

        //User wants to quit the game.
        case 'Q':
        case 'q':
            return 0;  //When 0 returned to main(), game loop ends, teardown() called.
            break;     //Control flow exits updateWorld(), returns to main().

	}


	//Add pieces to the board with respect to selcted column.
    //Whenever a piece is added, checkWinner() checks for winning move.
 
    //If top spot in a column has a piece, column is full, cannot add a piece there.
	if(*(board+columnTop) != '.'){
		printf("Column is full, choose another.");
        return 4;
        
    //If the base of the column is empty, put piece in that spot.
	}else if(*(board+columnBase)=='.'){
		*(board+columnBase) = *ppiece;
        int winner = checkWinner( (columnBase) ); //checkWinner() returns 0 if there is a winner.
        if(winner==0){
            return 2; //Somebody won the game.
        }
    //Otherwise add new piece on top of highest existing one in the column.   
	}else{
		
        //Check every spot in column until an existing piece is found.
		for(int n=columnTop; n<rows*columns; n= (n+7) ){
			
            //When highest piece is found, add new piece above it. 
			if( *(board+n)=='X' || *(board+n)=='O' ){
			    *(board+n-7)= *ppiece; 
                int winner = checkWinner( n-7 ); //Returns 0 if there is a winner.
                if(winner==0){
                    return 2;
                }
                break; //Solved bug. Before break; piece switching used to happen
			}
		}
    }
    //Checking countMoves==42 is more efficient than checking if every column is full 
    if(countMoves==42){
        return 3; //Board full.
    }

    //Unless winner is crowned (or whole board is full) return 1 and game loop continues.
	return 1;
}

/*
 * Print the game world to screen, reflecting the latest move.
 */
void displayWorld(){

    //Print every position in the board
    //After 7 positions, skip to new line to achieve rectangular board shape.
    for (int k=0; k<rows*columns; k++){
		if(k%7 == 0){
			printf("\n%c  ", *(board+k));
		}else{
			printf("%c  ", *(board+k));
		}
    }
    printf("\n"); //Formatting.
}

/**
 * Set up the client. 
 * Will not work unless server is set up FIRST.
 * @param IP address of server.
 * @param port number of server.
 */
int client(char *IP, char *port){  
    
    printf("\nSetting up client...");
    
    //Now use getaddrInfo() to get server info based on client() args.
    
    //Hints structure
    struct addrinfo hints, *addreturn; //*node not needed bc we only get one thing returned.
    char string[1000];
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC; //Use IPV4 or IPV6.       
    hints.ai_socktype = SOCK_STREAM; //Use TCP

    //Valid IP of server needed.
    //This gets address info of SERVER, so we can call out to the server.
    getaddrinfo(IP, port, &hints, &addreturn); 
    
    //Make ai_addr more specific.
    //struct sockaddr_in *in = (struct sockaddr_in *) addreturn->ai_addr;
    
    //If address of current response is IPV4
    if(addreturn->ai_addr->sa_family == AF_INET){
        
        //Use cast to make cur->ai_addr more specific.
        //because ai_addr is a generic pointer to structure.
        struct sockaddr_in *in = (struct sockaddr_in *) addreturn->ai_addr;
    
    }else{
        //IPV6
        struct sockaddr_in6 *in = (struct sockaddr_in6 *)addreturn->ai_addr;
    }

    //Create socket
    int connected;
    *psocketDescriptor = socket(addreturn->ai_family, addreturn->ai_socktype, addreturn->ai_protocol);
    connected = connect(*psocketDescriptor, addreturn->ai_addr, addreturn->ai_addrlen);
    
    //Check results
    if(*psocketDescriptor != -1){
        printf("\nSocket created.");
    }else{
        printf("Error creating socket: %s\n", strerror(errno));
    }
     
    if(connected != -1){
        printf("\nConnected to server.");
        printf("\nWaiting for server...\n");
    }else{
        printf("Error connecting to server: %s\n", strerror(errno));
    }
    
    //Receive server name
    int received = recv(*psocketDescriptor, serverName, 1000, 0);
    if(received==0){
        printf("Server disconnected, ending game.\n"); //Linguistic ambiguity.
        teardown();
        return 1;
    }

    //Set client name, send it over.
    printf("Please enter a name for client: ");
    fgets(clientName, 1000, stdin);

    //Check that player names are not identical.
    //If identical, client must choose new name.
    while(strcmp(serverName, clientName)==0){   
        printf("\nThe server is already named %sPlease choose a different name: ", serverName);
        fgets(clientName, 1000, stdin); //No need for memset(), fgets() overwrites.
    }
    int sent;
    sent = send(*psocketDescriptor, clientName, 1000, 0);

    //The connection is done, don't need addrinfo anymore.
    freeaddrinfo(addreturn);

    return 0;
}

/**
 * Set up the server.
 * @param port number to set up with.
 */
int server(char *port){
    
    printf("\nSetting up server...");

    //Use getaddrInfo(), return will be used to setup server

    //Hints stuff.
    struct addrinfo hints, *addreturn;
    memset(&hints, 0, sizeof(hints));
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    //Our own IP (belonging to server) is auto-filled.
    getaddrinfo(NULL, port, &hints, &addreturn); //Ports below 1024 may require SUDO.

    //Make socket
    int socketDescriptor;
    int connected;
    
    socketDescriptor = socket(addreturn->ai_family, addreturn->ai_socktype, addreturn->ai_protocol);
    if(socketDescriptor==-1){
        printf("\nError creating socket: %s\n", strerror(errno));
        return 1;
    }else{
        printf("\nSocket created.");
    }

    //Bind our socket to server address (from addrinfo)
    int bound;
    bound = bind(socketDescriptor, addreturn->ai_addr, addreturn->ai_addrlen);
    if(bound != -1){
        printf("\nSocket successfully bound to address.");
    }else{
        printf("\nBind() error: %s", strerror(errno));
        return 1;
    }

    //Listen - we are ready to recieve connection.
    int listening;
    listening = listen(socketDescriptor, 3);
    if(listening != -1){
        printf("\nListening...");
    }else{
        printf("\nListen() error: %s", strerror(errno));
        return 1;
    }

    //For accepting connection.
    struct sockaddr_in their_addr;
    socklen_t sin_size;
    
    printf("\n"); //Needed to flush buffer.
    
    sin_size = sizeof(their_addr);
    *pnewfd = accept(socketDescriptor, (struct sockaddr *) &their_addr, &sin_size);
    if(*pnewfd==-1){
        printf("\nAccept() error: %s\n", strerror(errno));
        return 1;
    }else{
        printf("\nServer connected to client.\n");
        //*pexitServerSend = 0; //New connection so reset to zero (if it changed at all).
    }

    //Set server name, send it over
    printf("Please enter a name for server: ");
    fgets(serverName, 1000, stdin); //fgets() allows string with whitespace to be received.
    int sent = send(*pnewfd, serverName, 1000, 0);
    
    //Receive client name
    printf("Waiting for client to select name...\n");
    int received = recv(*pnewfd, clientName, 1000, 0);
    if(received==0){
        printf("Client disconnected, ending game.\n");
        teardown();
        return 1;
    }

    freeaddrinfo(addreturn);
    return 0;
}

/*
 * Check whether there is a winner.
 * There are four possible ways a new move could complete a four-in-a-row:
 * Horizontal, vertical, diagonal up, diagonal down.
 * Return 0 if there's a winner.
 * Return 1 if there is no winner.
 */
int checkWinner(int index){
	
	//Keep track of how many pieces in a row are found during checkWinner().
	int fourRow = 0;
	int fourColumn = 0;
	int fourDiagDown = 0;  //Diagonal starting upper left (negative slope).
	int fourDiagUp = 0;  //Diagonal starting lower left (positive slope).

	/****************
	Check horizontal
	****************/
	int leftEdge = index - index%7;
    int rightEdge = leftEdge +6;
    
	for(int i = leftEdge; i < rightEdge+1; i++){ 

		//Check whether row has four consecutive.
		//Return 0 if yes.
		if(*(board+i)== *ppiece ){ 
			fourRow++;
			if(fourRow==4){
				return 0;
			}
		}else{
			fourRow = 0; //Consecutives are broken, count returns to zero;
		}
    }

	/****************
	Check vertical
	****************/
	for(int i = columnTop; i<= columnTop+35; i = i+7){

		if( *(board+i)== *ppiece){
			fourColumn++;
				if(fourColumn==4){
					return 0;
				}
		}else{
			fourColumn = 0;
		}
	}

	/*******************
	Check diagonal down
	*******************/
	int upperLeft;

	//If these indexes are confusing, see comment at the top of cfourmain.c
	for(int i = index; i>6; i= i-8){           
		
		if (i%7==0 && i<28){
			upperLeft = i;
			//Now we've found upper left of diag, go down the diag looking for four in a row.
			for(int j=upperLeft; j<48; j= j+8 ){
				if( *(board+j)== *ppiece){
					fourDiagDown++;
					if(fourDiagDown==4){
						return 0;
					}
				}else{
					fourDiagDown = 0;
				}
			}
		}else if(i==8){
			for(int j=8; j<49; j= j+8){
				if( *(board+j) == *ppiece ){
					fourDiagDown++;
					if(fourDiagDown==4){
						return 0;
					}
				}else{
					fourDiagDown=0;
				}
			}
		}else if (i==9){
			for(int k=9; k<42; k= k+8){
				if( *(board+k)== *ppiece ){
					fourDiagDown++;
					if(fourDiagDown==4){
						return 0;
					}
				}else{
					fourDiagDown=0;
				}
			}
		}else if (i==10){
			for(int p=10; p<35; p= p+8){
				if( *(board+p) == *ppiece ){
					fourDiagDown++;
					if(fourDiagDown==4){
						return 0;
					}
				}else{
					fourDiagDown = 0;
				}
			}
		}
	}

	/****************
	Check diagonal up
	*****************/
	int topRight;

	for(int i = index; i>6; i= i-6){
		
		if(i%7==6 && i<34){
			topRight = i;
			for(int j=topRight; j<49; j= j+6){
				if( *(board+j)== *ppiece){
					fourDiagUp++;
					if(fourDiagUp==4){
						return 0;
					}
				}else{
					fourDiagUp = 0;
				}
			}
		
		}else if(i==12){
			for(int k=12; k<43; k= k+6){
				if( *(board+k)== *ppiece){
					fourDiagUp++;
					if(fourDiagUp==4){
						return 0;
					}
				}else{
                    fourDiagUp = 0;                          
                }                  
			}
		}else if(i==11){
			for(int l=11; l<36; l= l+6){
				if( *(board+l)== *(ppiece) ){
					fourDiagUp++;
					if(fourDiagUp==4){
						return 0;
					}
				}else{
					fourDiagUp = 0;
				}
			}
		}else if(i==10){
			for(int m=10; m<29; m= m+6){
				if( *(board+m)== *ppiece){
					fourDiagUp++;
					if(fourDiagUp==4){
						return 0;
					}
				}else{
					fourDiagUp = 0;
				}
			}
		}
	}
	//If none of the four checks resulted in a winner (return 0),
	//then control flow falls through and returns 1 and game continues.
	return 1;
}

/**
 * Free game board space at end of game.
 * Called when there is a winner, a player quit, or the game board is full.
 */
void teardown(){
    
    int freeBoard, closeServerSD, closeClientSD;
    printf("\nDestroying the game.\n");
    
    //Free network resources.
    free(board);
    close(socketDescriptor);
    close(newfd);
    if(freeBoard==0 && closeServerSD==0 && closeClientSD==0){
        printf("Network resources freed.\n");
    }
    printf("\nGame over.\n\n");
}

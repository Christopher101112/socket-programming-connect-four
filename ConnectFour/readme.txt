2021
Connect Four 2-player (remote connection) game
@Author = ChristopherPouch


DEMONSTRATION
To see a 2-minute YouTube video explaining and demonstrating this program, follow this link:

  
CONTENTS
This program is fully functioning and is comprised of:
“connectfour.c”,
“cfourhreader.h”


OVERVIEW
This project is an implementation of the classic Connect Four game, and uses socket programming to connect two players remotely. The UI (or lack thereof) is basic text output to the console. One player acts as server, the other as client (see 'how to run' below). This program is based upon the classic game loop model, and continues until a player quits, the board is full, or there is a winner.


HOW TO RUN
*Playing this game requires one player to act as server and the other as client. This could take the form of two separate computers, or separate terminal windows on the same computer.
*If using separate computers, firewall settings may prevent remote connection.

1) Set up the server:
-Compile connectfour.c (with cfourheader.h in the same directory) command: "GCC connectfour.c"
-Execute the program with param port number (any int above 1023.  command: "./a.out 2000"
-wait for a client to connect (console output will notify of connection).

2) Set up the client and connect to server:
-Open a new terminal window or separate computer.
-Compile connectfour.c (with cfourheader.h in the same directory) command: "GCC connectfour.c"
-Execute the program with params IPV4 of SERVER, and port number (must match server's). command "./a.out 999.999.9.99 2000"

3) After connection, both players are prompted to set up and play the game. Follow the console instructions.



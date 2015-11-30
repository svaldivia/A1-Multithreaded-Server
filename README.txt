Author: Sebastian Valdivia
ID: 47373113
CSID: b7e8

Description
-------------------

This repo contains a multithreaded server using pThreads and a client to test its operations.

Server:
- Run the server using: ./mtserver <num_client_connections> <port_number>
Operations:
-Load: Returns number of connections
-Add: Returns the addition of the provided numbers
-Uptime: Returns the server uptime (Not working at the moment)

Client:
-Run the client using: ./client <client_host>
-Operations are hard coded in the client, this is mainly to test the functionality of the server.

testclient.py:
-TA provided client that I modified for testing my server.
-In general, it was to test the server faster than with client.c


My implementation of multithreaded server and the test client was based on the following
tutorials:

pThreads simple example:
http://timmurphy.org/2010/05/04/pthreads-in-c-a-minimal-working-example/

Beej’s Guide:
http://beej.us/guide/bgnet/output/html/singlepage/bgnet.html#theory

Single Threaded Example:
http://www.mario-konrad.ch/wiki/doku.php?id=programming:single_threaded_server

pThread in c server example:
http://www.binarytides.com/socket-programming-c-linux-tutorial/


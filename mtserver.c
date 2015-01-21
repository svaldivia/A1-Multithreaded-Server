/*
 ** mtserver.c -- Multi threaded server
 *
 * Based on Beej's Guide to Network Programming
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <pthread.h>
#include <stdbool.h>
#include <time.h>
#include "Thread.h"

#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 100

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/* Child sever function
 * 
 * @param int input_sockfd: socket to send & receive info
 */
void *childFunction(void * input_sockfd){
    int numbytes;
    char buf[MAXDATASIZE];
    bool connectionAlive = true;
    // Requests
    char uptimeR[6] = "uptime";
    char loadR[4] = "load";
    char exitR[4] = "exit";
    long response;
    
    
    
    int sockfd = *((int *)input_sockfd);
    
    while (connectionAlive) {
        if ((numbytes = recv(sockfd,buf,MAXDATASIZE-1,0)) == -1) {
            perror("recv");
            exit(1);
        }
        
        // Check if client closed connection
        if (numbytes == 0){
            break;
        }
        
        // Evaluate input
        if((*buf == *uptimeR) == 1){
            printf("Received Uptime Request\n");
            
            int time = 10;
            int network_order;
            network_order = htonl(time);
            
            printf("no: %d int: %d\n",network_order , ntohl(network_order));
            if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                perror("send");
            
        } else if((*buf == *loadR) == 1){
            printf("Received Load Request\n");
            if (send(sockfd, "Temp", 4, 0) == -1)
                perror("send");
            
        } else if((*buf == *exitR) == 1){
            connectionAlive = false;
        } else {
            printf("stuff lalala!\n");
        }
        
    }
    // Close connection
    close(sockfd);
    return NULL;
}





/* Main server function
 *
 * @param char *argv[1]: Max number of clients that can connect
 * @param char *argv[2]: Server port number
 */
int main(int argc, char **argv) {
    int maxConn,port;
    
    // Check parameters and validate
    if (argc > 1){
        // Check max connections
        if((maxConn = atoi(argv[1])) == 0) {
            fprintf(stderr, "Max connections is either not an integer or 0\n");
            return 1;
        }else if( maxConn <= 0){
            fprintf(stderr, "Max connections needs to be positive \n");
            return 1;
        }
        
        // Check Port number
        if (!argv[2]) {
            fprintf(stderr, "No port provided \n");
            return 1;
        }
        if((port = atoi(argv[2])) == 0){
            fprintf(stderr, "Port number invalid \n");
            return 1;
        } else if(port < 1024) {
            fprintf(stderr, "Port number invalid: Below 1024 \n");
            return 1;
        }
    } else {
        // No parameters provided
        fprintf(stderr, "No parameters provided \n");
        return 1;
    }
    
    // Print parameters
    int i;
    for (i = 0; i < argc; i++) {
        printf("Arg %d is: %s\n", i, argv[i]);
    }
    
    
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;
    struct sigaction sa;
    int yes=1;
    char s[INET6_ADDRSTRLEN];
    int rv;
    
    // Fill up hints address relevant info
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC; // Unespecified IP version
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = AI_PASSIVE; // use my IP
    
    // Set up--------------
    // getaddrinfo(hostname,port_number,hints,results)
    if ((rv = getaddrinfo(NULL, argv[2], &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // Set up socket info-----------
    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        // Get the file descriptor for the socket
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("server: socket");
            continue;
        }
        
        // Allow reuse of the socket port
        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes,
                       sizeof(int)) == -1) {
            perror("setsockopt");
            exit(1);
        }
        
        // Bind socket to port
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("server: bind");
            continue;
        }
        
        break;
    }
    
    if (p == NULL)  {
        fprintf(stderr, "server: failed to bind\n");
        return 2;
    }
    
    freeaddrinfo(servinfo); // all done with this structure
    
    // Listen for upcoming connections
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    
    printf("server: waiting for connections...\n");
    
    int currentConnections;
    
    while(1) {  // main accept() loop
        sin_size = sizeof their_addr;
        new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);
        if (new_fd == -1) {
            perror("accept");
            continue;
        }
        
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        currentConnections++;
        printf("Current connections: %d\n",currentConnections);
        
        //Create thread
//        if(void *childThread = createThread(childThread, &new_fd)){
//            fprintf(stderr,"Error creating Thread\n");
//            return 1;
//        }
//        void *childThread = createThread(childFunction, &new_fd);
//
//        runThread(&childThread,NULL);
        
        
        // Variable to reference thread
        pthread_t childT;
        
        // Create thread
        if(pthread_create(&childT, NULL,childFunction,&new_fd)){
            fprintf(stderr,"Error creating thread\n");
            return 1;
        }
        
        // Join thread
        if(pthread_join(childT,NULL)){
            fprintf(stderr, "Error joining thread\n");
            return 2;
            
        }
        
        
        currentConnections--;
        close(new_fd);  // parent doesn't need this
    }
    
    return 0;

}

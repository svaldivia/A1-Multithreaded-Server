/*
 ** mtserver.c -- Multi threaded server
 *
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
//#include <sys/sysinfo.h>


#define BACKLOG 10     // how many pending connections queue will hold
#define MAXDATASIZE 100

void *childFunction(void *);

// Global variable
int currentConnections = 0;
pthread_mutex_t mutexA; //Lock

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
    
    // Create thread array
    pthread_t thr[maxConn];
    // Initialize lock for global variable
    pthread_mutex_init(&mutexA,NULL);
    
    // Variable to reference thread
    pthread_t childT;
    
    sin_size = sizeof their_addr;
    
    while((new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size))) {  // main accept() loop
        // Connection accepted-------
        
        inet_ntop(their_addr.ss_family,
                  get_in_addr((struct sockaddr *)&their_addr),
                  s, sizeof s);
        printf("server: got connection from %s\n", s);
        
        // Update current connections
        currentConnections++;
        printf("server: Current connections: %d\n",currentConnections);
        
        if(currentConnections > maxConn){
            printf("server: Connection limit reached, closing connection...\n");
            currentConnections--;
            close(new_fd);
        } else {
            
            // Create thread
            if(pthread_create(&childT, NULL,childFunction,&new_fd)){
                fprintf(stderr,"Error creating thread\n");
                return 1;
            }
            
            // Detach Thread
            if(pthread_detach(childT)){
                fprintf(stderr, "Error detaching thread\n");
                return 2;
            }
        }
        
    }
    
    if (new_fd < 0) {
        perror("accept");
        return 1;
    }
    
    return 0;

}


/* Child sever function
 *
 * @param int input_sockfd: socket to send & receive info
 */
void *childFunction(void * input_sockfd){
    int numbytes;
    char buf[MAXDATASIZE];
    bool connectionAlive = true;
    bool invalidRequest = false;
    int invalidCount = 0;
    
    
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
        printf("Bytes received %d\n",numbytes);
        
        // Loop through received bytes
        
        int j;
        int add;
        for (j = 0; j < numbytes; j++) {
            
            invalidRequest = false;
            // Invalid input
            if (!(buf[j] == 'l' || buf[j] == 'e' || buf[j] == 'u' || (buf[j] >= '0' && buf[j] <= '9'))) {
                // invalid request
                invalidCount++;
                invalidRequest = true;
            }
            
            // Send invalid input message
            if(invalidRequest){
                printf("server: Invalid input: %d \n",invalidCount);
                int network_order = htonl(-1);
                if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                    perror("send");
                invalidRequest = false;
            }
            
            // Check invalid requests
            if (invalidCount > 2) {
                printf("server: Too many invalid requests\n");
                connectionAlive = false;
                break;
            }
            // Check if there are more bytes
            if(j >= numbytes){
                break;
            }
            
            //-------------------
            //     Add Digits
            //-------------------
            
            // Check if digit
            if (buf[j] >= '0' && buf[j] <= '9') {
                add = 0;
                add += buf[j]-48;
                for (j = j+1; j < numbytes; j++) {
                    // Digit?
                    if (buf[j] >= '0' && buf[j] <= '9') {
                        printf("add = %d\n", add);
                        add += buf[j]-48;
                    } else if (buf[j] == ' ') {
                        // Send addition
                        printf("server: Received Add digits Request. Addition = %d \n",add);
                        
                        int network_order = htonl(add);
                        if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                            perror("send");
                        
                        break;
                        
                    } else {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                        break;
                    }
                }
                
                if (j >= numbytes && buf[j-1] != ' ' && invalidRequest == false) {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                }
            }
            // Send invalid input message
            if(invalidRequest){
                printf("server: Invalid input: %d \n",invalidCount);
                int network_order = htonl(-1);
                if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                    perror("send");
                invalidRequest = false;
            }
            
            // Check invalid requests
            if (invalidCount > 2) {
                printf("server: Too many invalid requests\n");
                connectionAlive = false;
                break;
            }
            // Check if there are more bytes
            if(j >= numbytes){
                break;
            }
            
            //------------------
            //       Load
            //------------------
            
            // Check if load
            if (buf[j] == 'l') {
                j++;
                if (j >= numbytes) {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                } else if (buf[j] == 'o'){
                    j++;
                    if (j >= numbytes) {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                    } else if(buf[j] == 'a'){
                        j++;
                        if (j >= numbytes) {
                            // invalid request
                            invalidCount++;
                            invalidRequest = true;
                        } else if(buf[j] == 'd'){
                            // Send load

                            printf("server: Received Load Request\n");
                            
                            int connections;
                            //Lock before using variable
                            pthread_mutex_lock (&mutexA);
                            connections = currentConnections;
                            pthread_mutex_unlock(&mutexA);
                            
                            int network_order;
                            network_order = htonl(connections);
                            if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                                perror("send");
                        } else {
                            // invalid request
                            invalidCount ++;
                            invalidRequest = true;
                        }
                    } else {
                        // invalid request
                        invalidCount ++;
                        invalidRequest = true;
                    }
                } else {
                    // invalid request
                    invalidCount ++;
                    invalidRequest = true;
                }
            }
            
            // Send invalid input message
            if(invalidRequest){
                printf("server: Invalid input: %d \n",invalidCount);
                int network_order = htonl(-1);
                if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                    perror("send");
                invalidRequest = false;
            }
            
            // Check invalid requests
            if (invalidCount > 2) {
                printf("server: Too many invalid requests\n");
                connectionAlive = false;
                break;
            }
            // Check if there are more bytes
            if(j >= numbytes){
                break;
            }
            
            //------------------
            //       Exit
            //------------------
            
            // Check if exit
            if (buf[j] == 'e') {
                printf("exit \n");
                j++;
                if (j >= numbytes) {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                } else if (buf[j] == 'x'){
                    j++;
                    if (j >= numbytes) {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                    } else if(buf[j] == 'i'){
                        j++;
                        if (j >= numbytes) {
                            // invalid request
                            invalidCount++;
                            invalidRequest = true;
                        } else if(buf[j] == 't'){
                            // Send exit
                            printf("server: Exit Request\n");
                            int network_order = htonl(0);
                            if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                                perror("send");
                            connectionAlive = false;
                            break;
                        } else {
                            // invalid request
                            invalidCount++;
                            invalidRequest = true;
                        }
                    } else {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                    }
                } else {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                }
            }
            
            // Send invalid input message
            if(invalidRequest){
                printf("server: Invalid input: %d \n",invalidCount);
                int network_order = htonl(-1);
                if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                    perror("send");
                invalidRequest = false;
            }
            
            // Check invalid requests
            if (invalidCount > 2) {
                printf("server: Too many invalid requests\n");
                connectionAlive = false;
                break;
            }
            // Check if there are more bytes
            if(j >= numbytes){
                break;
            }
            
            //------------------
            //       Uptime
            //------------------
            
            // Check if uptime
            if (buf[j] == 'u') {
                j++;
                if (j >= numbytes) {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                } else if (buf[j] == 'p'){
                    j++;
                    if (j >= numbytes) {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                    } else if(buf[j] == 't'){
                        j++;
                        if (j >= numbytes) {
                            // invalid request
                            invalidCount++;
                            invalidRequest = true;
                        } else if(buf[j] == 'i'){
                            j++;
                            if (j >= numbytes) {
                                // invalid request
                                invalidCount++;
                                invalidRequest = true;
                            } else if(buf[j] == 'm'){
                                j++;
                                if (j >= numbytes) {
                                    // invalid request
                                    invalidCount++;
                                    invalidRequest = true;
                                } else if(buf[j] == 'e'){
                                    // Send uptime
                                    printf("server: Received Uptime Request\n");
                                    
//                                    struct sysinfo info;
//                                    sysinfo(&info);
//                                    printf("Uptime = %ld\n", info.uptime);
//                                    int network_order;
//                                    network_order = htonl(info.uptime);
//                                    
//                                    if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
//                                        perror("send");
                                } else {
                                    // invalid request
                                    invalidCount++;
                                    invalidRequest = true;
                                }
                            } else {
                                // invalid request
                                invalidCount++;
                                invalidRequest = true;
                            }
                        } else {
                            // invalid request
                            invalidCount++;
                            invalidRequest = true;
                        }
                    } else {
                        // invalid request
                        invalidCount++;
                        invalidRequest = true;
                    }
                } else {
                    // invalid request
                    invalidCount++;
                    invalidRequest = true;
                }

            }
            
            
            // Send invalid input message
            if(invalidRequest){
                printf("server: Invalid input: %d \n",invalidCount);
                int network_order = htonl(-1);
                if (send(sockfd, &network_order, sizeof(network_order), 0) == -1)
                    perror("send");
                invalidRequest = false;
            }
        }
    }

    // Update current connections
    pthread_mutex_lock (&mutexA);
    currentConnections--;
    pthread_mutex_unlock(&mutexA);
    
    // Close connection----------
    printf("server: Closed connection to client\n");
    close(sockfd);
    pthread_exit(NULL);
    return NULL;
}

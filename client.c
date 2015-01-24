/*
 ** client.c -- a client to unit test the functions in mtserver.c
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define PORT "3490" // the port client will be connecting to

#define MAXDATASIZE 100 // max number of bytes we can get at once


// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }
    
    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

int main(int argc, char *argv[])
{
    int sockfd, numbytes;
    char buf[MAXDATASIZE];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    
    if (argc != 2) {
        fprintf(stderr,"usage: client hostname\n");
        exit(1);
    }
    
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    
    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }
    
    // loop through all the results and connect to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                             p->ai_protocol)) == -1) {
            perror("client: socket");
            continue;
        }
        
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        
        break;
    }
    
    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        return 2;
    }
    
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),
              s, sizeof s);
    printf("client: connecting to %s\n", s);
    
    freeaddrinfo(servinfo); // all done with this structure
    
    // Uptime
    printf("Sending uptime\n");
    if (send(sockfd, "uptime", 6, 0) == -1)
        perror("send");
    
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    printf("bytes read: %d\n",numbytes);

    
    int uptime = ntohl(*(int*)buf);
    
    printf("client: received %d\n",uptime);
    
    // Load
    printf("Sending load\n");
    if (send(sockfd, "load", 6, 0) == -1)
        perror("send");
    
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    printf("bytes read: %d\n",numbytes);
    
    int load = ntohl(*(int*)buf);
    
    printf("client: received %d\n",load);
    
    // Add digits
    printf("Sending 7777\n");
    if (send(sockfd, "7777 ", 5, 0) == -1)
        perror("send");
    
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    printf("bytes read: %d\n",numbytes);
    
    int result = ntohl(*(int*)buf);
    
    printf("client: received %d\n",result);
    
    //---------------------------------------------
    // The following requests can't all run,
    // after 2 invalid requests the server closes
    // the connection and the last request receives
    // a -1 response. If one of invalid request is
    // commented, the exit works as expected.
    //---------------------------------------------
    
    // 2 invalid requests
    printf("Sending ads98h32 invalid request 1\n");
    if (send(sockfd, "ads98h32", 8, 0) == -1)
        perror("send");
    
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    printf("bytes read: %d\n",numbytes);
    
    int invalid1 = ntohl(*(int*)buf);
    
    printf("client: received %d\n",invalid1);
    
    
    printf("Sending 123d45 invalid request 2\n");
    if (send(sockfd, "123d45", 6, 0) == -1)
        perror("send");
    
    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
        perror("recv");
        exit(1);
    }
    
    printf("bytes read: %d\n",numbytes);
    
    int invalid2 = ntohl(*(int*)buf);
    
    printf("client: received %d\n",invalid2);
    
    
//    // Exit
//    printf("Sending exit\n");
//    if (send(sockfd, "exit",4, 0) == -1)
//        perror("send");
//    
//    if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
//        perror("recv");
//        exit(1);
//    }
//    
//    printf("bytes read: %d\n",numbytes);
//    
//    int exitR = ntohl(*(int*)buf);
//    
//    printf("client: received %d\n",exitR);
    
    
    
    
    close(sockfd);
    
    return 0;
}
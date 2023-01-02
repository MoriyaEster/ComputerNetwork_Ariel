#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define PORT 9000
#define SOURCE_IP "127.0.0.1"

int main()
{
    printf("hello watchdog\n");

    //opening a socket for the sender
    int sock = socket(AF_INET, SOCK_STREAM, 0);

    //check if there is no exception
    if (sock == -1) {
        printf("Could not create socket : %d\n", errno);
        return -1;
    }

    // used for IPv4 communication
    struct sockaddr_in recv_addr;
    memset(&recv_addr, 0, sizeof(recv_addr));

    recv_addr.sin_family = AF_INET; // Address family, AF_INET unsigned 
    recv_addr.sin_port = htons(PORT); // receiver port number 
    recv_addr.sin_addr.s_addr  = INADDR_ANY; // Internet address
    int rval = inet_pton(AF_INET,SOURCE_IP, &recv_addr.sin_addr);  // convert IPv4 addresses from text to binary form

    //check if there is no exception
    if (rval <= 0) {
        printf("inet_pton() failed\n");
        return -1;
    } 

    // Bind the socket to the port with any IP
    int bindResult = bind(sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr));

    printf("watchdog bind\n");

	//check if there is no exception 
    if (bindResult == -1) {
        printf("Bind failed with error code : %d\n", errno);
        // close the socket
        close(sock);
        return -1;
    }

    // Make the socket listening
    int listenResult = listen(sock, 2);

	//check if there is no exception
    if (listenResult == -1) {
        printf("listen() failed with error code : %d", errno);
        // close the socket
        close(sock);
        return -1;
    }

    printf("watchdog listen\n");

    //Accept and incoming connection
    struct sockaddr_in senderAddress;
    socklen_t senderAddressLen = sizeof(senderAddress);

    memset(&senderAddress, 0, sizeof(senderAddress));
    senderAddressLen = sizeof(senderAddress);
    int senderSocket = accept(sock, (struct sockaddr *)&senderAddress, &senderAddressLen);

    //check if there is no exception
    if (senderSocket == -1) {
        printf("listen failed with error code : %d", errno);
        // close the sockets
        close(sock);
        return -1;
    }

    //timer
    clock_t start, end;

    // save the starting time
    start = clock();

    int get = 0;
    char isOK = '1';

    while ( (end = clock()) - start < 10000 );
    {
        printf ("Enter the while\n");
        send(sock, &isOK ,sizeof(isOK), 0);
        printf("watchdog sent is OK\n");

        if (get == 1){
            start = clock();
            printf ("Enter the if\n");

        }
        recv(sock, &get, sizeof(get), MSG_DONTWAIT);
        printf ("Enter2 the while\n");
        
    }

    isOK = 0;
    send(sock, &isOK,sizeof(isOK), 0);
    printf ("exit the while\n");

    return 0;
}
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

#define PORT 3000
#define SOURCE_IP "127.0.0.1"

int main()
{
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
    recv_addr.sin_port = htons(PORT); // watchdog port number 
    recv_addr.sin_addr.s_addr  = INADDR_ANY; // Internet address
    int rval = inet_pton(AF_INET,"127.0.0.1", &recv_addr.sin_addr);  // convert IPv4 addresses from text to binary form

    //check if there is no exception
    if (rval <= 0) {
        printf("inet_pton() failed\n");
        return -1;
    } 

    // Bind the socket to the port with any IP
    int bindResult = bind(sock, (struct sockaddr *)&recv_addr, sizeof(recv_addr));

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

    int get = 0; //var which save if the better_ping got pong
    int isOK = 1; // var which sent to the better_ping "you can sent a ping"
    int sent = 0; // var will contain the number of bytes the send send
    int recv2 = 1; // var will ask if we got message in the recv

    //save the times
    struct timeval start, end;
    gettimeofday(&start, 0);

    // save the end time (just for the first ping)
    gettimeofday(&end, 0);

    float milliseconds = 0;
    while ( recv2 > 0)
    {
        sent = send (senderSocket, &isOK ,sizeof(isOK), 0);

        if (get == 1){
            //save the time the better_ping send "i got a pong" so the timer will restart 
            gettimeofday(&start, 0);
        }

        //while we didnt get any message from the better_ping and the timer didnt got to 10 try to recv
        recv2 = 0;
        while (((milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f) < 11000) && recv2 <= 0)
        {
            recv2 = recv(senderSocket, &get, sizeof(get), MSG_DONTWAIT); 
            gettimeofday(&end, 0);
        }


    }

    //if we didn't get a message from the better_ping "I got a pong" and pass 10 minutes, sent to better_ping to kill his program
    isOK = 0;
    send(senderSocket, &isOK,sizeof(isOK), 0);
    close (senderSocket);
    close(sock);

    return 0;
}
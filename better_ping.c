#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h> // gettimeofday()
#include <sys/types.h>
#include <stdbool.h>
#include <netinet/tcp.h>
#include <signal.h>
#include <time.h>

#define SOURCE_IP "127.0.0.1"
#define PORT 3000

// IPv4 header len without options
#define IP4_HDRLEN 20

// ICMP header len for echo req
#define ICMP_HDRLEN 8

//destination ip length
#define IP_LEN 16

// Checksum algo
unsigned short calculate_checksum(unsigned short *paddress, int len);

// run 2 programs using fork + exec
// command: make clean && make all && ./partb
int main(int argc, char ** argv)
{
    char *args[2];
    // compiled watchdog.c by makefile
    args[0] = "./watchdog";
    args[1] = NULL;
    int status;
    int pid = fork();
    if (pid == 0)
    {
        printf("in child \n");
        execvp(args[0], args);
    }
    else{

        sleep(2);

        //ping
        int yes = 1;

        //The user enter an IP to ping
        char * destination_ip = argv[1];

        //check if the IP is valid
        struct in_addr addr;
        int error = inet_pton(AF_INET, destination_ip, &addr); //return a positive num if the IP is valid

        if (error <= 0) {
            printf("Error: Invalid IP address.\n");
            yes = 0;
        } else {
            printf("IP address is valid.\n");
        }

        if (yes){
            //opening a TCP socket
            int sock = socket(AF_INET, SOCK_STREAM, 0);

            //check if there is no exception
            if (sock == -1) {
                printf("Could not create socket : %d", errno);
                return 1;
            }

            // used for IPv4 communication
            struct sockaddr_in serv_addr;
            memset(&serv_addr, 0, sizeof(serv_addr));

            serv_addr.sin_family = AF_INET; // Address family, AF_INET unsigned 
            serv_addr.sin_port = htons(PORT); // Port number  
            serv_addr.sin_addr.s_addr = INADDR_ANY; // Internet address

            // Make a connection to the watchdog with socket SendingSocket.
            int connectResult = connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr));

            //check if there is no exception
            if (connectResult == -1) {
                printf("connect() failed with error code : %d\n", errno);
                // cleanup the socket;
                close(sock);
                return -1;
            }


            struct icmp icmphdr; // ICMP-header
            // Sequence Number (16 bits): starts at 0
            icmphdr.icmp_seq = 0;

            // Create raw socket for IP-RAW (make IP-header by yourself)
            int raw_sock = -1; 
            if (yes && (raw_sock = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) == -1)
            {
                fprintf(stderr, "socket() failed with error: %d\n", errno);
                fprintf(stderr, "To create a raw socket, the process needs to be run by Admin/root user.\n\n");
                return -1;
            }

            //endless loop ping-pong
            while (yes)
            {
                //wait for a while so that the system doesn't throw us out
                sleep (1);

                char data[IP_MAXPACKET] = "This is the ping.\n";

                int datalen = strlen(data) + 1;
                //===================
                // ICMP header
                //===================

                // Message Type (8 bits): ICMP_ECHO_REQUEST
                icmphdr.icmp_type = ICMP_ECHO;

                // Message Code (8 bits): echo request
                icmphdr.icmp_code = 0;

                // Identifier (16 bits): some number to trace the response.
                // It will be copied to the response packet and used to map response to the request sent earlier.
                // Thus, it serves as a Transaction-ID when we need to make "ping"
                icmphdr.icmp_id = 18;

                // ICMP header checksum (16 bits): set to 0 not to include into checksum calculation
                icmphdr.icmp_cksum = 0;

                // Combine the packet
                char packet[IP_MAXPACKET];


                // Next, ICMP header
                memcpy((packet), &icmphdr, ICMP_HDRLEN);

                // After ICMP header, add the ICMP data.
                memcpy(packet + ICMP_HDRLEN, data, datalen);

                // Calculate the ICMP header checksum
                icmphdr.icmp_cksum = calculate_checksum((unsigned short *)(packet), ICMP_HDRLEN + datalen);
                memcpy((packet), &icmphdr, ICMP_HDRLEN);

                struct sockaddr_in dest_in;
                memset(&dest_in, 0, sizeof(struct sockaddr_in));
                dest_in.sin_family = AF_INET;

                // The port is irrelvant for Networking
                dest_in.sin_addr.s_addr = inet_addr(destination_ip);
                inet_pton(AF_INET, destination_ip, &(dest_in.sin_addr.s_addr));

                //save the time for RTT calculate
                struct timeval start, end;
                gettimeofday(&start, 0);

                //recv from the watchdog if to continue or to exit
                int isOK = 0;
                int recvISOK = recv(sock, &isOK, sizeof(isOK), 0);

                //with that var it will sent to the watchdog that got pong
                int sentStart = 1;

                if(isOK){

                    // Send the packet using sendto() for sending datagrams.
                    int bytes_sent = sendto(raw_sock, packet, ICMP_HDRLEN + datalen, 0, (struct sockaddr *)&dest_in, sizeof(dest_in));
                    if (bytes_sent == -1)
                    {
                        fprintf(stderr, "sendto() failed with error: %d", errno);
                        return -1;
                    }

                    // Get the ping response
                    socklen_t len = sizeof(dest_in);
                    ssize_t bytes_received = 0;

                    /* while the watchdog didnt sent to kill the program and we didnt get pong
                    continue to try get a pong or message for the watchdog to kill the program  */
                    while((bytes_received <= 0) && isOK)
                    {
                        bytes_received = recvfrom(raw_sock, packet, sizeof(packet), MSG_DONTWAIT, (struct sockaddr *)&dest_in, &len);
                        if (bytes_received > 0)
                        {
                        break;
                        }
                        recvISOK = recv(sock, &isOK, sizeof(isOK), MSG_DONTWAIT);

                    }
                    if (isOK == 0){
                        break;
                    }

                    //sent in TCP to watchdog that the ping is going to sent now so start the timer
                    int sentStartTimer = send(sock, &sentStart ,sizeof(sentStart), 0);

                    //save the ip destination to ping
                    char ip_to_ping [IP_LEN];

                    //save the details from the packet
                    struct iphdr *ip_header = (struct iphdr *) packet;
                    struct icmphdr *icmp_header = (struct icmphdr *) (packet + sizeof(struct iphdr));

                    //convert the ip from binary
                    inet_ntop(AF_INET, &ip_header->saddr, ip_to_ping, IP_LEN);

                    //print the ip to ping and the seq num
                    printf("IP : %s, Packet sequence number: %d\n",ip_to_ping, icmphdr.icmp_seq);

                    //save the time for RTT calculate
                    gettimeofday(&end, 0);

                    //calculate the RTT and print
                    float milliseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec) / 1000.0f;
                    unsigned long microseconds = (end.tv_sec - start.tv_sec) * 1000.0f + (end.tv_usec - start.tv_usec);
                    printf("TIME: %f milliseconds (%ld microseconds)\n\n", milliseconds, microseconds);

                    //increase the seq num
                    icmphdr.icmp_seq++;

                    bzero(packet, IP_MAXPACKET);
                }
                else {
                    break;
                }

            }
            printf ("Server ip: %s cannot be reached.\n", destination_ip);
            close(raw_sock);
            close(sock);
        }
    }

    //end of ping
    wait(&status); // waiting for child to finish before exiting
    printf("child exit status is: %d\n", status);
    return 0;
}

// Compute checksum (RFC 1071).
unsigned short calculate_checksum(unsigned short *paddress, int len)
{
    int nleft = len;
    int sum = 0;
    unsigned short *w = paddress;
    unsigned short answer = 0;

    while (nleft > 1)
    {
        sum += *w++;
        nleft -= 2;
    }

    if (nleft == 1)
    {
        *((unsigned char *)&answer) = *((unsigned char *)w);
        sum += answer;
    }

    // add back carry outs from top 16 bits to low 16 bits
    sum = (sum >> 16) + (sum & 0xffff); // add hi 16 to low 16
    sum += (sum >> 16);                 // add carry
    answer = ~sum;                      // truncate to 16 bits

    return answer;
}
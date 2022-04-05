#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <poll.h>
#include <stdbool.h>

#define SERVER_PORTNO 10123 //self defined port number
#define PACKET_STARTID 0xFFFF
#define MY_CLIENTID 0x47 // my client ID
#define MAX_CLIENTID 0xFF // max client id size
#define PACKET_ENDID 0xFFFF
#define MAX_LEN 0xFF
#define DATA 0xFFF1
#define ACK 0xFFF2
#define REJECT 0xFFF3
#define REJECTSUBCODE1 0xFFF4 // out of sequenc
#define REJECTSUBCODE2 0xFFF5 // len mismatch
#define REJECTSUBCODE3 0xFFF6 // missing end of packet
#define REJECTSUBCODE4 0xFFF7 // duplcate packet

//return packets sent to client
typedef struct ackpacket{
    short packet_startid;
    char my_clientid;
    short ackn;
    char received_segno;
    short packet_endid;
} ackpacket;

typedef struct return_packet{
    short packet_startid;
    char my_clientid;
    short reject;
    short reject_subcode;
    char received_segno;
    short packet_endid;
} return_packet;

// packets received from client
typedef struct data_received_packet {
    short packet_startid;
    char my_clientid;
    short data;
    char segment_no;
    char len;
    char pload[255];
    short packet_endid;
} data_received_packet;

int main(void)
{
    // create receive and return variables
    data_received_packet dataarray;
    return_packet responsearray;
    ackpacket ackresponse;

    struct sockaddr_in remote_addr; // remote address
    socklen_t addresslen = sizeof(remote_addr); // len of addresses
    int fd;  // our socket
    
    // creating a socket
    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) ==-1) {
        perror("socket creation error");
    }

    //Setting up remote address // bind the socket to local ip address and a specific port
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = htonl(INADDR_ANY);/* htonl converts a long integer (e.g. address) to a network representation */
    remote_addr.sin_port = htons(SERVER_PORTNO);/* htons converts a short integer (e.g. port) to a network representation */
    bind(fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
    
    // Initial server run setup
    int initial_run = 1;
    printf("\nServer: Waiting for connection on port %d", SERVER_PORTNO);
    printf("\n----------------------------------------------------------------\n");
        
    // initializing polling for counter reset
    struct pollfd pollfdsocket;
    pollfdsocket.fd = fd;
    pollfdsocket.events = POLLIN;
    
    int lastsegment = -1;
    int latestsegment;
    int polling_val;    
    
    // main loop which never exits
    int i=0;
    while(i==0){
        // Reseting packet counter after 2 seconds
        if(initial_run!=1){
            polling_val = poll(&pollfdsocket,1,2000);
            switch (polling_val){
            case 0:
                printf("\n\nServer: Connection timed out. Resetting received packet counter.");
                printf("\nServer: Waiting for connection");
                printf("\n----------------------------------------------------------------\n");
                lastsegment = -1;
                break;
            }
        }
        
        // Receiving packet from client
        if (recvfrom(fd, &dataarray, sizeof(data_received_packet), 0, (struct sockaddr *)&remote_addr, &addresslen) <= 0) printf("Server: Received error!\n");
        else printf("Server: Received message: \"%s\"\n", dataarray.pload);
        
        // Setting for secondary connections
        initial_run = 0;
        
        //Initialize common attributes in packet to send to client
        responsearray.packet_startid = PACKET_STARTID;
        responsearray.my_clientid = MY_CLIENTID;
        responsearray.reject = REJECT;
        responsearray.received_segno = dataarray.segment_no;
        responsearray.packet_endid = PACKET_ENDID;
        
        latestsegment = dataarray.segment_no;
        int rej_type;
        
        //Error/Ack decision logic
        if(lastsegment==latestsegment){rej_type=4;}else if((lastsegment + 1) != latestsegment){rej_type=1;}else if(dataarray.packet_endid != (short)PACKET_ENDID){rej_type=3;}else if(dataarray.len != (char)sizeof(dataarray.pload)){rej_type=2;}
        else{
            rej_type=5;            
        }

        switch (rej_type){
        case 1:
            //out of order packet received error
            responsearray.reject_subcode = REJECTSUBCODE1;
            printf("Server: ERROR. Out of order packets. Was expecting segment_no %d but received segment_no %d.\n\n", lastsegment+1, latestsegment);
            break;
        case 2:
            //length mismatch error
            responsearray.reject_subcode = REJECTSUBCODE2;
            printf("Server: ERROR. Packet %d has a len mismatch.\n\n", latestsegment);
            break;
        case 3:
            //Invalid end of frame error
            responsearray.reject_subcode = REJECTSUBCODE3;
            printf("Server: ERROR. Packet %d has an invalid end of frame ID.\n\n", latestsegment);
            break;
        case 4:
            //duplicate packet error
            responsearray.reject_subcode = REJECTSUBCODE4;
            printf("Server: ERROR. Duplicate packets. Was expecting segment_no %d but receieved segment_no %d again.\n\n", lastsegment+1, latestsegment);
            break;
        case 5:
            //No errors, Send acknowledgment
            responsearray.reject = ACK;
            responsearray.reject_subcode = 0;
            printf("Server: ACK. Packet %d acknowledged.\n\n", latestsegment);
            break;
        }
                
        //save segment_no to check order of packets and validity
        lastsegment = latestsegment;
        
        //sending err/ack packet back to client
        sendto(fd, &responsearray, sizeof(return_packet), 0, (struct sockaddr *)&remote_addr, addresslen);
    }
}

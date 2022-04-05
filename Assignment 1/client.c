#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>

#define SERVER_PORTNO 10123 //self defined port number
#define NUM_PACKETS 5 // total no of packets
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

//repsonse packets received from server
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

//packets sent to server
typedef struct data_send_packet {
    short packet_startid;
    char my_clientid;
    short data;
    char segment_no;
    char len;
    char pload[255];
    short packet_endid;
} data_send_packet;

int successmsg(int);
// Function to print final success/termination message
int successmsg(int success)
{
    switch (success)
    {
    case 1:
        printf("Finished successfully\n");
        break;
    default:
        printf("\nPacket was still unacknowledged after 3 retransmissions");
        printf("\nServer does not respond.\n");
        break;
    }
    return 0;
}

int main(void)
{
    int i;
    data_send_packet packet_dataarray[NUM_PACKETS];
    
    struct sockaddr_in remote_addr; // remote address
    int fd;
    data_send_packet swap;
    int no_sendattempts = 1;
    int slen=sizeof(remote_addr);

    // creating a socket
    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))<0){
        perror("socket cannot be created");
        return 0;
    }
    
    // setting up remote address
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(SERVER_PORTNO);
    
    // Introducing errors
    // Case 0: Ideal Transmission i.e. order is 0, 1, 2, 3, 4
    // Case 1: Out of order packet is sent i.e. order is 0, 1, 3, 2, 4
    // Case 2: Length mismatch i.e. The 3rd packet's len field is set to 0x07; actual len is 0xFF
    // Case 3: Invalid end of frame i.e. End of frame identifier for 2nd packet is set to 0xFFF0; actual is 0xFFFF
    // Case 4: Duplicate packet i.e. order is 0, 1, 1, 3, 4
    int error_handling = 2;
    
    // Initial packet setup
    int n=0;
    char temp[2048];
    while(n<NUM_PACKETS){
        packet_dataarray[n].packet_startid = PACKET_STARTID;
        packet_dataarray[n].my_clientid=MY_CLIENTID;
        packet_dataarray[n].data = DATA;
        packet_dataarray[n].segment_no = n;
        sprintf(temp, "Packet Number %d", n);
        strncpy(packet_dataarray[n].pload, temp, 255);
        packet_dataarray[n].len = sizeof(packet_dataarray[n].pload);
        packet_dataarray[n].packet_endid = PACKET_ENDID;
        n=n+1;
    }    
    
    // Setting up errors for error handling
    printf("\n\n");
    if(error_handling==1){
        swap = packet_dataarray[2];
        packet_dataarray[2] = packet_dataarray[3];
        packet_dataarray[3] = swap;
        printf("Test for out of order packet\n");
    }
    else if (error_handling==2){
        packet_dataarray[2].len = 0x07;
        printf("Test for length mismatch \n");
    }
    else if (error_handling==3){
        packet_dataarray[1].packet_endid = 0xFFF0;
        printf("Test for wrong EOF\n");
    }
    else if (error_handling==4){
        packet_dataarray[2] = packet_dataarray[1];
        printf("Test for duplicate packet\n");
    }
    else{
        printf("Test for ideal transmission\n");
    }
    printf("----------------------------------------------------------------\n\n");
    
    
    // create receive and return variables
    return_packet responsearray;
    ackpacket ackresponse;
    data_send_packet data;

    //  setting pollfd for ack_timer
    struct pollfd pfdsock;
    pfdsock.fd = fd;
    pfdsock.events = POLLIN;    

    int polling_val;
    //  loop to send data packet to server and fetch response
    for (i=0; i<NUM_PACKETS; i++){
        data = packet_dataarray[i];
        printf("Client: Sending packet %d. Attempt 1\n", i);
        
        if (sendto(fd, &data, sizeof(data_send_packet), 0, (struct sockaddr *)&remote_addr, slen)==-1) {
            perror("Client: Sendto error:\n");
            return 1;
        }
        
        //ack_timer setup
        for (no_sendattempts=2;no_sendattempts<=3;){
        
            //poll for 3.5 seconds, and resend up to 2 times
            polling_val = poll(&pfdsock,1,3500);
            if(polling_val == 0) {
                //timeout, must resend
                printf("Client: No response from server. Sending again. Attempt %d\n", no_sendattempts);
                no_sendattempts=no_sendattempts+1;
            }
            else {
                // receiving response from server
                recvfrom(fd, &responsearray, sizeof(return_packet), 0, (struct sockaddr *)&remote_addr, &slen);
                if(responsearray.reject == (short)ACK){ //ACK received!
                    printf("Server: Packet %d acknowledged.\n\n", i);
                    break;
                }
                else { //Error received! determine what error occured
                    short rejsub=responsearray.reject_subcode;
                    short sub1=(short)REJECTSUBCODE1;
                    short sub2=(short)REJECTSUBCODE2;
                    short sub3=(short)REJECTSUBCODE3;
                    short sub4=(short)REJECTSUBCODE4;
                    int rej_type;
                    if(rejsub==sub1)rej_type=1;else if (rejsub==sub2)rej_type=2;else if (rejsub==sub3)rej_type=3;else if (rejsub==sub4)rej_type=4;
                    switch (rej_type){
                        case 1:
                            printf("Server: ERROR. Out of order packets. Was expecting segment_no %d but received segment_no %d.\n\n", i, responsearray.received_segno);
                            return -1;
                        case 2:
                            printf("Server: ERROR. Packet %d has a len mismatch.\n\n", i);
                            return -1;
                        case 3:
                            printf("Server: ERROR. Packet %d has an invalid end of frame ID.\n\n", i);
                            return -1;
                        case 4:
                            printf("Server: ERROR. Duplicate packets. Was expecting segment_no %d but receieved segment_no %d again.\n\n", i, responsearray.received_segno);
                            return -1;
                    }
                }
            }
        }
        if(no_sendattempts > 3) break;// Exiting loop after 3 attempts
    }
    
    int success = no_sendattempts<=3?1:0;
    successmsg(success);
}
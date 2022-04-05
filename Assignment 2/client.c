#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>

#define SERVER_PORTNO 10123 //self defined port number
#define CHECKNO 4 //no of users phone to be checked
#define PACKET_STARTID 0xFFFF
#define PACKET_ENDID 0xFFFF
#define MY_CLIENTID 0x47 // my client ID
#define MAX_CLIENTID 0xFF // max client id size
#define MAX_LEN 0xFF

#define ACCESSPER 0xFFF8 //request access
#define NOTPAID 0xFFF9 // account not paid
#define NOTEXIST 0xFFFA // account doesnt exist
#define ACCESSOK 0xFFFB // access granted

//data packet sent to network
typedef struct data_send_packet {
    short packet_startid;
    unsigned char my_clientid;
    short packet_datatype; //ACCESSOK, ACCESSPER, NOTPAID, or NOTEXIST
    char segment_no;
    unsigned char len;
    char technology;
    unsigned long subs_no;
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
        printf("\nClient: Server not responding even after 3 attempts.\n");
        printf("\nServer does not respond.\n");
        break;
    }
    return 0;
}

int main(void)
{
    int i;
    unsigned long numtocheck[CHECKNO];
    char technology[CHECKNO];

    // Initializing numbers to check with technology
    numtocheck[0] = 4083902387; technology[0]= 0x02; //2g // unpaid
    numtocheck[1] = 4083992399; technology[1]= 0x04; //4g // technology mismatch
    numtocheck[2] = 4083002300; technology[2]= 0x02; //2g // does not exist
    numtocheck[3] = 4083902378; technology[3]= 0x03; //3g // successful access

    data_send_packet packet_dataarray[CHECKNO];

    // Creating data send and data receive packet variables
    data_send_packet data_receive;
    data_send_packet data_send;

    struct sockaddr_in remote_addr;    // remote address
    int fd;
    int slen=sizeof(remote_addr);
    
    // creating a socket
    if ((fd=socket(AF_INET, SOCK_DGRAM, 0))<0){
        perror("socket cannot be created");
        return 0;
    }
        
    // setting up remote address
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_port = htons(SERVER_PORTNO);/* htons converts a short integer (e.g. port) to a network representation */
    
    int n=0;
    while(n<CHECKNO){
        unsigned long length=sizeof(packet_dataarray[n].technology) + sizeof(packet_dataarray[n].my_clientid);
        packet_dataarray[n].packet_startid = PACKET_STARTID;
        packet_dataarray[n].my_clientid= MY_CLIENTID;
        packet_dataarray[n].packet_datatype = ACCESSPER;
        packet_dataarray[n].segment_no = n;
        packet_dataarray[n].technology = technology[n];
        packet_dataarray[n].len = length;
        packet_dataarray[n].subs_no = numtocheck[n];
        packet_dataarray[n].packet_endid = PACKET_ENDID;
        n=n+1;
    }    
    
    i = 0;
    int polling_val;
    int no_sendattempts;

    
    printf("\nClient Starting transmission");
    printf("\n----------------------------------------------------------\n");
    
    //  setting pollfd for ack_timer
    struct pollfd pfdsock;
    pfdsock.fd = fd;
    pfdsock.events = POLLIN;

    //  loop to send data packet to server and fetch response
    for (i=0; i<CHECKNO; i++){
        data_send = packet_dataarray[i];
        printf("Client: Requesting access for number %lu from server. Attempt 1\n", data_send.subs_no);
        
        //Sending packet to network
        sendto(fd, &data_send, sizeof(data_send_packet), 0, (struct sockaddr *)&remote_addr, slen);
        
        //ack_timer setup
        for (no_sendattempts=2;no_sendattempts<=3;){
            //poll for 3.5 seconds, and resend up to 2 times
            polling_val = poll(&pfdsock,1,3500);
            if(polling_val == 0) {
                //no response, resend
                printf("\nClient: Server not responding for request access %lu.\nSending again. Attempt %d\n", data_send.subs_no, no_sendattempts);                
                no_sendattempts=no_sendattempts+1;
            }
            else {
                // receiving response from network
                recvfrom(fd, &data_receive, sizeof(data_send_packet), 0, (struct sockaddr *)&remote_addr, &slen);
                short dtype=data_receive.packet_datatype;
                short acceso=(short)ACCESSOK;
                short unpaid=(short)NOTPAID;
                short noex=(short)NOTEXIST;
                if(dtype == acceso){
                    printf("Network: %lu network access successful\nSUCCESS\n\n", data_receive.subs_no);
                    break;
                }
                else if(dtype == unpaid){
                    printf("Network: %lu has not paid for network access.\nFAIL\n\n", data_receive.subs_no);
                    break;
                }
                else{
                    if(data_receive.technology == 0x00){
                        printf("Network: %lu is not present in database.\nFAIL\n\n", data_receive.subs_no);
                        break;
                    }
                    else{
                        printf("Network: %lu has technology mismatch.\nFAIL\n\n", data_receive.subs_no);
                        break;
                    }
                }
            }
        }
        // Exiting loop after 3 attempts
        if(no_sendattempts > 3) break;
    }

    int success = no_sendattempts<=3?1:0;
    successmsg(success);
}

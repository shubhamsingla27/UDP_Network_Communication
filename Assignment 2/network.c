#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <poll.h>
#include <sys/socket.h>
#include <netdb.h>
#include <math.h>
#include <arpa/inet.h>
#include <stdbool.h> //allows for boolean

#define SERVER_PORTNO 10123 //self defined port number
#define PACKET_STARTID 0xFFFF
#define MY_CLIENTID 0x47 // my client ID
#define MAX_CLIENTID 0xFF // max client id size
#define MAX_LEN 0xFF
#define PACKET_ENDID 0xFFFF

#define ACCESSPER 0xFFF8 //request access
#define NOTPAID 0xFFF9 // account not paid
#define NOTEXIST 0xFFFA // account doesnt exist
#define ACCESSOK 0xFFFB // access granted

void linklistinsert(unsigned long, char, char);
int fileget(char *);

//structure of packets
typedef struct data_received_packet {
    short packet_startid;
    unsigned char my_clientid;
    short packet_datatype; //ACCESSOK, ACCESSPER, NOTPAID, or NOTEXIST
    char segment_no;
    unsigned char len;
    char technology;
    unsigned long subs_no;
    short packet_endid;
} data_received_packet;

//node structure for reading database i.e, linked list
typedef struct node
{
    unsigned long subs_no;
    char technology;
    char payment;
    struct node *next;
}NODEVAL;

//variable declare for linked list
NODEVAL *head = NULL;
NODEVAL *current = NULL;

NODEVAL * findval(unsigned long);
//function to search link list for requested value
NODEVAL * findval (unsigned long valrec){
    NODEVAL *v=head;
    do {
        if (v->subs_no!=valrec) v=v->next;
        else return v;
    } while(v!=NULL);
    return NULL;//if not found return null
}

// populating the linked list by opening the file and importing
int fileget(char *x){
    unsigned long subs_no;
    unsigned int technology;
    unsigned int payment;
    
    //opening the file
    FILE *fp;
    fp = fopen(x,"r");
    //passin each line values to linklistinsert() function
    while((fscanf(fp,"%lu %x %x", &subs_no, &technology, &payment) >0)){
        linklistinsert(subs_no, (char)technology, (char)payment);
    }
    fclose(fp);
    return 1;
}

//takes each line of data from the file, imports it to linked list
void linklistinsert(unsigned long sub, char technology, char payment) {
    //creating temp node of same size as nodeval
    NODEVAL *tempentry=(NODEVAL *)malloc(sizeof(NODEVAL));
    //each line's data is fed into temp node
    tempentry->subs_no = sub;tempentry->technology = technology;tempentry->payment = payment;
    
    //adding to end of list if not first node
    if (head != NULL){
        current->next = tempentry;
        current = tempentry;
        current->next = NULL;
    }
    //making temp node head if first node
    else {
        head = tempentry;
        current = tempentry;
        current->next = NULL;
    }
}

int main(int argc, char **argv)
{
    struct sockaddr_in remote_addr;
    socklen_t addresslen = sizeof(remote_addr);// length of addresses
    int fd;  // our socket
    data_received_packet dataarray;
    data_received_packet responsearray;
    NODEVAL* findval_res;
    
    // putting values in linked list from database file
    fileget("databasefile.txt");
    
    // creating socket
    fd = socket(AF_INET, SOCK_DGRAM, 0);
    
    //Setting up the remote address // bind the socket to local ip address and a specific port
    remote_addr.sin_family = AF_INET;
    remote_addr.sin_addr.s_addr = htonl(INADDR_ANY);/* htonl converts a long integer (e.g. address) to a network representation */
    remote_addr.sin_port = htons(SERVER_PORTNO);/* htons converts a short integer (e.g. port) to a network representation */
    bind(fd, (struct sockaddr *)&remote_addr, sizeof(remote_addr));
    
    printf("\nNetwork:Ready on port %d for client connection!\n", SERVER_PORTNO);
    printf("-----------------------------------------------------\n");
    
    // main loop which never exits
    int i=0;
    while(i==0){

        // receiving data packets from client 
        if (recvfrom(fd, &dataarray, sizeof(data_received_packet), 0, (struct sockaddr *)&remote_addr, &addresslen) <= 0) printf("Network: Receive error!\n");
        else printf("Network:Received request for: \"%lu\"\n", dataarray.subs_no);
        
        //Initialize common attributes in packet to send to client
        responsearray.packet_startid = PACKET_STARTID;
        responsearray.packet_endid = PACKET_ENDID;
        responsearray.my_clientid = MY_CLIENTID;
        responsearray.segment_no = dataarray.segment_no;
        responsearray.len = dataarray.len;
        responsearray.subs_no = dataarray.subs_no;
        int rej_type;
        findval_res = findval(dataarray.subs_no);
        if(findval_res == NULL){rej_type=1;}else if(findval_res->technology != dataarray.technology){rej_type=2;}else if(findval_res->payment == 0){rej_type=3;}
        else{rej_type=4;}
        switch (rej_type)
        {
        case 1:
            responsearray.packet_datatype = NOTEXIST;
            responsearray.technology = 0x00;
            printf("Network:User is not present in database.\nFAIL\n\n");
            break;
        case 2:
            responsearray.technology = findval_res->technology;
            responsearray.packet_datatype = NOTEXIST;
            printf("Network:User has technology mismatch.\nFAIL\n\n");
            break;
        case 3:
            responsearray.technology = findval_res->technology;
            responsearray.packet_datatype = NOTPAID;
            printf("Network:User has not paid for network accessd\nFAIL\n\n");
            break;
        case 4:
            responsearray.technology = findval_res->technology;
            responsearray.packet_datatype = ACCESSOK;
            printf("Network:Access successful\nSUCCESS\n\n");
            break;
        }       
        //sending err/ack packet back to client
        sendto(fd, &responsearray, sizeof(data_received_packet), 0, (struct sockaddr *)&remote_addr, addresslen);
    }
}
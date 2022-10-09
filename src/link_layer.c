// Link layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include <signal.h>
#include "link_layer.h"

#define FALSE 0
#define TRUE 1

#define BUF_SIZE 256
#define FLAG 0x7E
#define CSET 0x03
#define CUA 0x07
#define CRR 0x05
#define A 0x03
#define BCC1 A^CSET
#define BCC2 A^CUA
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

typedef enum
{
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK,
    DATA
} StateEnum;

volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int fd;

void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;
    unsigned char set[]={FLAG,A,CSET,(A^CSET),FLAG};
    write(fd, set, 5);
    printf("Alarm #%d\n", alarmCount);
    printf("enable: %d\n", alarmEnabled);
}

int llopen(LinkLayer connectionParameters)
{
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0)
    {
        perror(connectionParameters.serialPort);
        exit(-1);
    }

    struct termios oldtio;
    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1)
    {
        perror("tcgetattr");
        exit(-1);
    }

    // Clear struct for new port settings
    memset(&newtio, 0, sizeof(newtio));

    newtio.c_cflag = connectionParameters.baudRate | CS8 | CLOCAL | CREAD;
    newtio.c_iflag = IGNPAR;
    newtio.c_oflag = 0;

    // Set input mode (non-canonical, no echo,...)
    newtio.c_lflag = 0;
    newtio.c_cc[VTIME] = 0.1; // Inter-character timer unused
    newtio.c_cc[VMIN] = 0;  // Blocking read until 5 chars received[0]

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received[0] but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received[0] but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure sett\n");
    if(connectionParameters.role==LlTx){
        unsigned char set[]={FLAG,A,CSET,(A^CSET),FLAG};
        //int received = write(fd, set, 5);
        write(fd, set, 5);
        int done = FALSE;
        (void)signal(SIGALRM, alarmHandler);
        unsigned char ua[5]={0};
        StateEnum state = START;
        while (alarmCount < 4 && !done)
        {
            if (alarmEnabled == FALSE)
            {
                alarm(3); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;
            }
            unsigned char received[1];
            int bytes = read(fd, received, 1);
            if(bytes==-1){
                printf("error\n");
                continue;
            }
            if(bytes==0)
                continue; 
            printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));

            switch(state){
                case START:
                    if(received[0] == FLAG){
                        ua[0]=received[0];
                        state=FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if(received[0] == FLAG)
                        continue;
                    else if (received[0] != A ){
                        state = START;
                    }
                    else{ 
                        ua[1]=received[0];
                        state=A_RCV;
                    }
                    break;
                case A_RCV:
                    if(received[0] == FLAG){
                        state=FLAG_RCV;
                        ua[0]=received[0];
                    }
                    else if (received[0] != CUA ){
                        state = START;
                    }
                    else{
                        ua[2]=received[0];
                        state=C_RCV;
                    }
                    break;
                case C_RCV:
                    if (received[0] != (ua[1]^ua[2]) ){
                        state = START;
                    }
                    else{
                        ua[3]=received[0];
                        state = BCC_OK;
                    }
                    break;
                case BCC_OK:
                    if(received[0] != FLAG){
                        state = START;
                    }
                    else{
                        ua[4]=received[0];
                        done=TRUE;
                    }
                    break;
            }

        }
        alarm(0);
        printf("Ending program\n");
    }
    else if(connectionParameters.role==LlRx){
        unsigned char set[5]={0};
        StateEnum state = START;
        int running = 1;
        while (running)
        {
            unsigned char received[1];
            int bytes = read(fd, received, 1);
            if(bytes==-1){
                printf("error\n");
                continue;
            }
            printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));
            switch(state){
                case START:
                    if(received[0] == FLAG){
                        set[0]=received[0];
                        state=FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if(received[0] == FLAG)
                        continue;
                    else if (received[0] != A ){
                        state = START;
                    }
                    else{
                        set[1]=received[0];
                        state=A_RCV;
                    }
                    break;
                case A_RCV:
                    if(received[0] == FLAG){
                        state=FLAG_RCV;
                        set[0]=received[0];
                    }   
                    else if (received[0] != CSET ){
                        state = START;
                    }
                    else{
                        set[2]=received[0];
                        state=C_RCV;
                    }
                    break;
                case C_RCV:
                    if (received[0] != (set[1]^set[2]) ){
                        state = START;
                    }
                    else{
                        set[3]=received[0];
                        state = BCC_OK;
                    }
                    break;
                case BCC_OK:
                    if(received[0] != FLAG){
                        state = START;
                    }
                    else{
                        set[4]=received[0];
                        running=0;
                    }
                    break;
            }
        }
        printf("done\n");
        unsigned char ua[]={FLAG,A,CUA,(A^CUA),FLAG};
        write(fd, ua, 5);
    }
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    int done = FALSE;
    (void)signal(SIGALRM, alarmHandler);
    StateEnum state = START;
    int aux=0;
    while (alarmCount < 4 && !done)
    {
        // write info
        unsigned char i[BUF_SIZE+1]={0};
        i[0]= FLAG;
        i[1] = A;
        i[2] = CSET;
        i[3] = A ^ CSET;

        if(bufSize<250){
            int j=0;
            for(; j<bufSize; j++){
                i[j+4]=buf[j+aux];
            }
            i[j+1]= FLAG;
            i[j+2]= A ^ CSET;
        }
        else {
            for (int j = 0; j < 250; j++) {
                i[j + 4] = buf[j+aux];
            }
            i[BUF_SIZE]= FLAG;
            i[BUF_SIZE-1]= A ^ CSET;;
        }
        
        write(fd, i, BUF_SIZE);

        //read
        if (alarmEnabled == FALSE)
        {
            alarm(3); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
        unsigned char received[1];
        int bytes = read(fd, received, 1);
        if(bytes==-1){
            printf("error\n");
            continue;
        }
        printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));
        unsigned char ack[5]={0};
        switch(state){
            case START:
                if(received[0] == FLAG){
                    ack[0]=received[0];
                    state=FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if(received[0] == FLAG)
                    continue;
                else if (received[0] != A ){
                    state = START;
                }
                else{
                    ack[1]=received[0];
                    state=A_RCV;
                }
                break;
            case A_RCV:
                if(received[0] == FLAG){
                    state=FLAG_RCV;
                    ack[0]=received[0];
                }
                else if (received[0] != CRR){
                    state = START;
                }
                else{
                    ack[2]=received[0];
                    state=C_RCV;
                }
                break;
            case C_RCV:
                if (received[0] != (ack[1]^ack[2]) ){
                    state = START;
                }
                else{
                    ack[3]=received[0];
                    state = BCC_OK;
                }
                break;
            case BCC_OK:
                if(received[0] != FLAG){
                    state = START;
                }
                else{
                    ack[4]=received[0];
                    alarm(0);
                    if(aux>=bufSize)
                        done=TRUE;
                    aux+=250;

                }
                break;
        }

    }

    printf("Ending program\n");

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    unsigned char i[BUF_SIZE+1]={0};
    StateEnum state = START;
    int running = 1;
    while (running)
    {
        unsigned char received[1];
        int bytes = read(fd, received, 1);
        if(bytes==-1){
            printf("error\n");
            continue;
        }
        printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));
        switch(state){
            case START:
                if(received[0] == FLAG){
                    i[0]=received[0];
                    state=FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if(received[0] == FLAG)
                    continue;
                else if (received[0] != A ){
                    state = START;
                }
                else{
                    i[1]=received[0];
                    state=A_RCV;
                }
                break;
            case A_RCV:
                if(received[0] == FLAG){
                    state=FLAG_RCV;
                    i[0]=received[0];
                }   
                else if (received[0] != CSET ){
                    state = START;
                }
                else{
                    i[2]=received[0];
                    state=C_RCV;
                }
                break;
            case C_RCV:
                if (received[0] != (i[1]^i[2]) ){
                    state = START;
                }
                else{
                    i[3]=received[0];
                    state = BCC_OK;
                }
                break;
            case BCC_OK:
                unsigned char buf[BUF_SIZE + 1] = {0};
                while (STOP == FALSE)
                {

                    // Returns after 5 chars have been input
                    int bytes = read(fd, buf, BUF_SIZE);
                    buf[bytes] = '\0'; // Set end of string to '\0', so we can printf

                    if (strlen(buf) != 0){
                        printf(":%s:%d\n", buf, bytes);
                    }
                    
                    if (buf[0] == i[1]^i[2])
                        STOP = TRUE;
                }

                if (buf[0] != (i[1]^i[2]) ){
                        state = START;
                    }
                    else{
                        i[3]=received[0];
                        state = BCC_OK;
                    }


                break;

        }
    }
    printf("done\n");
    unsigned char ua[]={FLAG,A,CUA,(A^CUA),FLAG};
    write(fd, ua, 5);
    return fd;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}

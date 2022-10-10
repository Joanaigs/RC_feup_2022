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
#define ESC 0x7D
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
                unsigned char set[]={FLAG,A,CSET,(A^CSET),FLAG};
                write(fd, set, 5);
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

    unsigned int count = 0;

    

    for (int k = 0; k < bufSize; k++){
        if(buf[k]==FLAG || buf[k] == ESC){
           count += 1;
        }
    }

    unsigned int size = bufSize + 6 + count;
    unsigned char i[size];
    for(int k=0; k<size; k++){
        i[k]=0;
    }
    while (alarmCount < 4 && !done)
    {

       // write info;
        i[0]= FLAG;
        i[1] = A;
        i[2] = CSET;
        i[3] = A ^ CSET;
        unsigned int bcc2 = 0, k = 0;
        for (int j = 0; j < bufSize; j++) {
            if(buf[j]==FLAG){
                i[j+4+k]=ESC;
                i[j+5 +k]= 0x5E;
                bcc2 ^= ESC;
                bcc2 ^= 0x5E;
                k++;

            }
            else if (buf[j] == ESC){
                i[j+4+k] = ESC;
                i [j+5+k] = 0x5D;
                bcc2 ^= ESC;
                bcc2 ^= 0x5D;
                k++;

            }
            else{
                i[j + 4+k] = buf[j];
                bcc2 ^= buf[j];
            }
            
        }
        i[size-1]= FLAG;
        i[size-2]= bcc2;

        
        write(fd, i, size);

        //READ

        if (alarmEnabled == FALSE)
        {
            write(fd, i, size);
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
                    done=TRUE;
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
    unsigned char fh[6]={0};
    StateEnum state = START;
    int running = TRUE;
    int esc_activated = FALSE;
    char data[MAX_PAYLOAD_SIZE] = {0};
    int data_size = 0;
    unsigned char bcc2 = 0;
    while (running)
    {
        //Verifies if everything is okay with byte received
        unsigned char received[1];
        int byte = read(fd, received, 1);
        if(byte==-1){
            printf("error\n");
            continue;
        }
        printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF)); // print do que recebe

        switch(state){
            case START:
                if(received[0] == FLAG){
                    fh[0]=received[0];
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
                    fh[1]=received[0];
                    state=A_RCV;
                }
                break;
            case A_RCV:
                if(received[0] == FLAG){
                    state=FLAG_RCV;
                    fh[0]=received[0];
                }   
                else if (received[0] != CSET ){
                    state = START;
                }
                else{
                    fh[2]=received[0];
                    state=C_RCV;
                }
                break;
            case C_RCV:
                if(received[0] == FLAG){
                    state=FLAG_RCV;
                    fh[0]=received[0];
                }
                else if (received[0] != (fh[1]^fh[2]) ){
                    state = START;
                }
                else{
                    fh[3]=received[0];
                    state = BCC_OK;
                }
                break;
            case BCC_OK:

                if(received[0] == FLAG){
                    
                    if (data[data_size - 1] == bcc2){
                        data[data_size - 1] = 0;
                        data_size--;
                        running = FALSE;
                    }else{
                        // mandar um NACK()
                    }
                   
                }
                else{

                    if (received[0] == ESC){
                        esc_activated = TRUE;
                        bcc2 ^= ESC;
                    }    
                    if (esc_activated){
                        if (received[0] == 0x5E){
                            data[data_size] = FLAG;
                            data_size++;
                            bcc2 ^= 0x5E;
                        }
                        if (received[0] == 0x5D){
                            data[data_size] = ESC;
                            data_size ++;
                            bcc2 ^= 0x5D;
                        }
                    }
                    else{
                        data[data_size] = received[0];
                        data_size ++;
                        bcc2 ^= received[0];
                    }

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

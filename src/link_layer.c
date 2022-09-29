// Link layer protocol implementation
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>
#include "link_layer.h"

// MISC
#define BUF_SIZE 256
#define FLAG 0x7e
#define CSET 0x03
#define CUA 0x07
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
    STOP
} StateEnum;

volatile int STOP = FALSE;
int llopen(LinkLayer connectionParameters)
{
    int fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

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
    newtio.c_cc[VTIME] = 0; // Inter-character timer unused
    newtio.c_cc[VMIN] = 1;  // Blocking read until 5 chars received

    // VTIME e VMIN should be changed in order to protect with a
    // timeout the reception of the following character(s)

    // Now clean the line and activate the settings for the port
    // tcflush() discards data written to the object referred to
    // by fd but not transmitted, or data received but not read,
    // depending on the value of queue_selector:
    //   TCIFLUSH - flushes data received but not read.
    tcflush(fd, TCIOFLUSH);

    // Set new port settings
    if (tcsetattr(fd, TCSANOW, &newtio) == -1)
    {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure set\n");

    if(connectionParameters.role==LlTx){
        unsigned char set[5]={FLAG,A,CSET,BCC1,FLAG};
        int bytes = write(fd, set, 5);
    }
    else if(connectionParameters.role==LlRx){
        unsigned char set[5]={0};
        StateEnum state = START;
        bool running =true;
        while (running)
        {
            unsigned char received;
            int bytes = read(fd, received, 1);
            printf("var=0x%02X\n", (unsigned int)(bytes & 0xFF));
            switch(state){
                case START:
                    if(received != FLAG)
                        continue;
                    else{
                        set[0]=received;
                        state=FLAG_RCV
                    }
                    break;
                case FLAG_RCV:
                    if(received == FLAG)
                        continue;
                    else if (received != A_RCV ){
                        state = START;
                    }
                    else{
                        set[1]=received;
                        state=A_RCV
                    }
                    break;
                case A_RCV:
                    if(received == FLAG){
                        state=FLAG_RCV;
                        set[0]=received
                    }   
                    else if (received != 1 ){
                        state = START;
                    }
                    else{
                        set[2]=received;
                        state=C_RCV
                    }
                    break;
                case C_RCV:
                    if (received != set[1]^set[2] ){
                        state = START;
                    }
                    else{
                        set[3]=received;
                        state = BCC_OK;
                    }
                    break;
                case BCC_OK:
                    if(received != FLAG){
                        state = START;
                    }
                    else{
                        set[4]=received;
                        state=STOP
                    }
                    break;
                case STOP:
                    running = false;
                    break;
            }
        }
        printf("done\n");
    }
    return fd;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet)
{
    // TODO

    return 0;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics)
{
    // TODO

    return 1;
}

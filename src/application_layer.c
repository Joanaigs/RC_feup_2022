// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer llayer;
    llayer.serialPort = serialPort;
    if(!strcmp(role,"tx"))
        llayer.role= LlTx;
    else if(!strcmp(role,"rx"))
        llayer.role=LlRx;
    llayer.timeout=timeout;
    llayer.baudRate=baudRate;
    llayer.nRetransmissions=nTries;
    printf("%s\n", role);
    printf("%d\n", llayer.role);
    llopen(llayer);

    if(llayer.role == LlTx){
        unsigned char buf[10] = {0};
        printf("Enter a string : ");
        gets(buf);
        llwrite(buf, 10);
    }

    else if(llayer.role == LlRx)
    {
        unsigned char buff[MAX_PAYLOAD_SIZE]={0};
        int i=llread(buff);
        printf("\n%d\n", i);
        for (int k = 0; k < i; k++){
            printf("var=0x%02X, unsigned char: %c\n", (unsigned int)(buff[k] & 0xFF), buff[k]);
    }
    }

}

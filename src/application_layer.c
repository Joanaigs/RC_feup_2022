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
}

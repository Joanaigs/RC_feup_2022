// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    LinkLayer llayer;
    llayer.serialPort = serialPort;
    llayer.role= LlTx;
    llayer.timeout=timeout;
    llayer.baudRate=baudRate;
    llayer.nRetransmissions=nTries;


    int fd = llopen(llayer);
}

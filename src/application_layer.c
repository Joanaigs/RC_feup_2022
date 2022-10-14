// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>
int numDigits(long n){
    int count=0;
    do {
        n /= 10;
        ++count;
    } while (n != 0);
    return count;
}

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
        /*unsigned char buf[10] = {0};
        printf("Enter a string : ");
        gets(buf);
        llwrite(buf, 10);*/

        // calculating the size of the file
        struct stat st;
        stat(filename, &st);
        off_t fileSize = st.st_size;


        int fileSize_bytes= numDigits(fileSize);
        char fileSizeStr[fileSize_bytes];
        sprintf(fileSizeStr, "%ld", fileSize);

        int fileName_bytes= strlen(filename) ;

        unsigned int controlPacketSize = 3 + fileSize_bytes+ 2 + fileName_bytes;
        unsigned char controlPacket[controlPacketSize];
        controlPacket[0]=2;
        controlPacket[1]=0;
        controlPacket[2]=fileSize_bytes;
        for(int i=0; i < fileSize_bytes; i++){
            controlPacket[i+3]= fileSizeStr[i];
        }
        controlPacket[3 + fileSize_bytes ]=1;
        controlPacket[3 + fileSize_bytes + 1]=fileName_bytes;
        for(int i=0; i < fileName_bytes; i++){
            controlPacket[3 + fileSize_bytes + 2 + i]=filename[i];
        }

        int a= llwrite(controlPacket, controlPacketSize);
        
        FILE* ptr, * ptr2;
        ptr = fopen(filename,"rb");
        ptr2 = fopen("a.png","wb");
        if (NULL == ptr) {
            printf("file can't be opened \n");
        }
        int n=0;
        unsigned char dataPacket[MAX_PAYLOAD_SIZE]={0};
        dataPacket[0]=1;
        while(TRUE) {
            if(fileSize>=MAX_PAYLOAD_SIZE-4){
                unsigned char buffer[MAX_PAYLOAD_SIZE-4]={0};
                fread(buffer, MAX_PAYLOAD_SIZE-4, 1, ptr);
                fileSize=fileSize-(MAX_PAYLOAD_SIZE-4);
                dataPacket[1]=n;
                dataPacket[2]=(MAX_PAYLOAD_SIZE-4)/256;
                dataPacket[3]=(MAX_PAYLOAD_SIZE-4)%256;
                for(int i=0; i<MAX_PAYLOAD_SIZE-4; i++){
                    dataPacket[i+4]=buffer[i];
                }
                n++;
                //llwrite(dataPacket, MAX_PAYLOAD_SIZE);
                //fwrite(buffer, MAX_PAYLOAD_SIZE-4, 1, ptr2);
            }
            else if(fileSize==0){
                break;
            }
            else{
                unsigned char buffer[fileSize];
                if(!fread(buffer, fileSize, 1, ptr)){
                    printf("osld");
                    break;
                }

                dataPacket[1]=n;
                dataPacket[2]=(fileSize-4)/256;
                dataPacket[3]=(fileSize-4)%256;
                for(int i=0; i<fileSize-4; i++){
                    dataPacket[i+4]=buffer[i];
                }
                n++;
                //llwrite(dataPacket, (int)fileSize);
                //fwrite(buffer, MAX_PAYLOAD_SIZE-4, 1, ptr2);
                break;
            }
        }

        // Closing the file
        fclose(ptr);
        //fclose(ptr2);

        controlPacket[0]=3;
        int j= llwrite(controlPacket, controlPacketSize);

    }

    else if(llayer.role == LlRx)
    {
        unsigned char buff[MAX_PAYLOAD_SIZE]={0};
        int i=llread(buff);
        int j=llread(buff);
        printf("\n%d\n", i);
        for (int k = 0; k < i; k++){
            printf("var=0x%02X, unsigned char: %c\n", (unsigned int)(buff[k] & 0xFF), buff[k]);
    }
    }

}

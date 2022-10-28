// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <limits.h>


int numDigits(long n) {
    int count = 0;
    do {
        n /= 10;
        ++count;
    } while (n != 0);
    return count;
}

void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename) {
    LinkLayer llayer;
    llayer.serialPort = serialPort;
    if (!strcmp(role, "tx"))
        llayer.role = LlTx;
    else if (!strcmp(role, "rx"))
        llayer.role = LlRx;
    llayer.timeout = timeout;
    llayer.baudRate = baudRate;
    llayer.nRetransmissions = nTries;
    llopen(llayer);

    if (llayer.role == LlTx) {
      
        printf("Sending data...\n");

        // calculating the size of the file
        struct stat st;
        stat(filename, &st);
        off_t fileSize = st.st_size;


        int fileSize_bytes = numDigits(fileSize);
        char fileSizeStr[fileSize_bytes];
        sprintf(fileSizeStr, "%ld", fileSize);

        int fileName_bytes = strlen(filename);

        unsigned int controlPacketSize = 3 + fileSize_bytes + 2 + fileName_bytes;
        unsigned char controlPacket[controlPacketSize];
        controlPacket[0] = 2;
        controlPacket[1] = 0;
        controlPacket[2] = fileSize_bytes;
        for (int i = 0; i < fileSize_bytes; i++) {
            controlPacket[i + 3] = fileSizeStr[i];
        }
        controlPacket[3 + fileSize_bytes] = 1;
        controlPacket[3 + fileSize_bytes + 1] = fileName_bytes;
        for (int i = 0; i < fileName_bytes; i++) {
            controlPacket[3 + fileSize_bytes + 2 + i] = filename[i];
        }

        if (llwrite(controlPacket, controlPacketSize) == -1) {
            printf("error in llwrite\n");
            return;
        }

        FILE *file;
        file = fopen(filename, "rb");
        if (NULL == file) {
            printf("file can't be opened \n");
        }
        int n = 0;
        unsigned char dataPacket[MAX_PAYLOAD_SIZE] = {0};
        dataPacket[0] = 1;
        while (TRUE) {
            if (fileSize >= MAX_PAYLOAD_SIZE - 4) {
                unsigned char buffer[MAX_PAYLOAD_SIZE - 4] = {0};
                fread(buffer, MAX_PAYLOAD_SIZE - 4, 1, file);
                fileSize = fileSize - (MAX_PAYLOAD_SIZE - 4);
                dataPacket[1] = n;
                dataPacket[3] = (MAX_PAYLOAD_SIZE - 4) / 256;
                dataPacket[2] = (MAX_PAYLOAD_SIZE - 4) % 256;
                for (int i = 0; i < MAX_PAYLOAD_SIZE - 4; i++) {
                    dataPacket[i + 4] = buffer[i];
                }
                n++;
                if (llwrite(dataPacket, MAX_PAYLOAD_SIZE) == -1) {
                    printf("error in llwrite\n");
                    return;
                }
            } else if (fileSize == 0) {
                break;
            } else {
                unsigned char buffer[fileSize];
                if (!fread(buffer, fileSize, 1, file)) {
                    printf("error reading file\n");
                    break;
                }

                dataPacket[1] = n;
                dataPacket[3] = (fileSize) / 256;
                dataPacket[2] = (fileSize) % 256;
                for (int i = 0; i < fileSize; i++) {
                    dataPacket[i + 4] = buffer[i];
                }
                n++;
                if (llwrite(dataPacket, (int) fileSize + 4) == -1) {
                    printf("error in llwrite\n");
                    return;
                }
                break;
            }
        }

        // Closing the file
        fclose(file);

        controlPacket[0] = 3;
        if (llwrite(controlPacket, controlPacketSize) == -1) {
            printf("error in llwrite\n");
            return;
        }

        printf("File sent!\n");

    } else if (llayer.role == LlRx) {
        FILE *file;
        file = fopen(filename, "wb");
        unsigned char buf[MAX_PAYLOAD_SIZE] = {0};
        unsigned char name[MAX_PAYLOAD_SIZE] = {0};
        unsigned char nameSize;
        unsigned char fileLength;
        unsigned char fileSize[MAX_PAYLOAD_SIZE];
        int count = 0;
        printf("Receiving data...\n");
        int size = llread(buf);
        if (size == -1) {
            printf("Error reading start control packet.\n");
        }
        if (buf[0] == 2) {
            if (buf[1] == 0) {
                fileLength = buf[2];
                for (int i = 0; i < fileLength; i++) {
                    fileSize[i] = buf[i + 3];

                }
            }
            if (buf[3 + fileLength] == 1) {
                nameSize = buf[4 + fileLength];
                for (int i = 0; i < nameSize; i++) {
                    name[i] = buf[i + 5 + fileLength];
                }
            }
            while (TRUE) {
                unsigned char info[MAX_PAYLOAD_SIZE] = {0};
                size = llread(info);

                if (size == -1) {
                    printf("Error reading control/data packet.\n");
                }
                if (info[0] == 3) {
                    unsigned char name2[MAX_PAYLOAD_SIZE] = {0};
                    unsigned char nameSize2;
                    unsigned char fileLength2;
                    unsigned char fileSize2[MAX_PAYLOAD_SIZE];
                    if (info[1] == 0) {
                        fileLength2 = info[2];
                        for (int i = 0; i < fileLength2; i++) {
                            fileSize2[i] = info[i + 3];

                        }
                    }
                    if (fileLength != fileLength2) {
                        printf("Start info and ending info don't match.\n");
                        return;
                    }
                    for (int i = 0; i < fileLength; i++) {
                        if (fileSize[i] != fileSize2[i]) {
                            printf("Start info and ending info don't match.\n");
                            return;
                        }
                    }
                    if (info[3 + fileLength2] == 1) {
                        nameSize2 = info[4 + fileLength2];
                        for (int i = 0; i < nameSize2; i++) {
                            name2[i] = info[i + 5 + fileLength2];
                        }
                    }
                    if (nameSize != nameSize2) {
                        printf("Start info and ending info don't match.\n");
                        return;
                    }
                    for (int i = 0; i < nameSize; i++) {
                        if (name[i] != name2[i]) {
                            printf("Start info and ending info don't match.\n");
                            return;
                        }
                    }
                    break;
                }
                unsigned char n = info[1];
                if (count != n) {
                    printf("sequence of files received not correct, please try again\n");
                }
                count++;
                unsigned char l1 = info[2];
                unsigned char l2 = info[3];
                unsigned long long k = l2 * 256 + l1;
                unsigned char data[k];
                for (int i = 0; i < k; i++) {
                    data[i] = info[i + 4];
                }
                fwrite(data, k, 1, file);
            }
        }

        printf("File received! \n");


    }

    
    if (llclose(TRUE) == -1) {
        printf("error in llclose\n");
    }

}

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
#define CREJ 0x01
#define A 0x03
#define ESC 0x7D
#define BCC1 A^CSET
#define BCC2 A^CUA
////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////

typedef enum {
    START,
    FLAG_RCV,
    A_RCV,
    C_RCV,
    BCC_OK
} StateEnum;

struct termios oldtio;
volatile int STOP = FALSE;
int alarmEnabled = FALSE;
int alarmCount = 0;
int fd;
int nTries = 0;
int timeout = 0;

void alarmHandler(int signal) {
    alarmEnabled = FALSE;
    alarmCount++;
    printf("Alarm #%d\n", alarmCount);
    printf("enable: %d\n", alarmEnabled);
}


int llopen(LinkLayer connectionParameters) {
    fd = open(connectionParameters.serialPort, O_RDWR | O_NOCTTY);

    if (fd < 0) {
        perror(connectionParameters.serialPort);
        exit(-1);
    }
    nTries = connectionParameters.nRetransmissions;
    timeout = connectionParameters.timeout;

    struct termios newtio;

    // Save current port settings
    if (tcgetattr(fd, &oldtio) == -1) {
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
    if (tcsetattr(fd, TCSANOW, &newtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    printf("New termios structure sett\n");
    if (connectionParameters.role == LlTx) {
        unsigned char set[] = {FLAG, A, CSET, (A ^ CSET), FLAG};
        int done = FALSE;
        (void) signal(SIGALRM, alarmHandler);
        unsigned char a, c;
        StateEnum state = START;
        while (alarmCount < nTries && !done) {
            if (alarmEnabled == FALSE) {
                state = START;
                if (write(fd, set, 5) == -1) {
                    printf("writing error\n");
                    return -1;
                }
                alarm(timeout); // Set alarm to be triggered in 3s
                alarmEnabled = TRUE;
            }
            unsigned char received[1];
            int bytes = read(fd, received, 1);
            if (bytes == -1) {
                printf("reading error in llopen\n");
                return -1;
            }
            if (bytes == 0)
                continue;
            //printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));

            switch (state) {
                case START:
                    if (received[0] == FLAG) {
                        state = FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if (received[0] == FLAG)
                        continue;
                    else if (received[0] != A) {
                        state = START;
                    } else {
                        a = received[0];
                        state = A_RCV;
                    }
                    break;
                case A_RCV:
                    if (received[0] == FLAG) {
                        state = FLAG_RCV;
                    } else if (received[0] != CUA) {
                        state = START;
                    } else {
                        c = received[0];
                        state = C_RCV;
                    }
                    break;
                case C_RCV:
                    if (received[0] != (a ^ c)) {
                        state = START;
                    } else {
                        state = BCC_OK;
                    }
                    break;
                case BCC_OK:
                    if (received[0] != FLAG) {
                        state = START;
                    } else {
                        done = TRUE;
                    }
                    break;
            }

        }
        alarm(0);
        alarmEnabled = FALSE;
        printf("Ending program\n");
    } else if (connectionParameters.role == LlRx) {
        unsigned char set[5] = {0};
        StateEnum state = START;
        int running = 1;
        while (running) {
            unsigned char received[1];
            int bytes = read(fd, received, 1);
            if (bytes == -1) {
                printf("reading error in llopen2\n");
                return -1;
            }
            //printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));
            switch (state) {
                case START:
                    if (received[0] == FLAG) {
                        set[0] = received[0];
                        state = FLAG_RCV;
                    }
                    break;
                case FLAG_RCV:
                    if (received[0] == FLAG)
                        continue;
                    else if (received[0] != A) {
                        state = START;
                    } else {
                        set[1] = received[0];
                        state = A_RCV;
                    }
                    break;
                case A_RCV:
                    if (received[0] == FLAG) {
                        state = FLAG_RCV;
                        set[0] = received[0];
                    } else if (received[0] != CSET) {
                        state = START;
                    } else {
                        set[2] = received[0];
                        state = C_RCV;
                    }
                    break;
                case C_RCV:
                    if (received[0] != (set[1] ^ set[2])) {
                        state = START;
                    } else {
                        set[3] = received[0];
                        state = BCC_OK;
                    }
                    break;
                case BCC_OK:
                    if (received[0] != FLAG) {
                        state = START;
                    } else {
                        set[4] = received[0];
                        running = 0;
                    }
                    break;
            }
        }
        unsigned char ua[] = {FLAG, A, CUA, (A ^ CUA), FLAG};
        if (write(fd, ua, 5) == -1) {
            printf("writing error\n");
            return -1;
        }
    }
    return 1;
}

////////////////////////////////////////////////
// LLWRITE
////////////////////////////////////////////////
int llwrite(const unsigned char *buf, int bufSize) {
    int done = FALSE;
    (void) signal(SIGALRM, alarmHandler);
    StateEnum state = START;

    unsigned int count = 0;


    for (int k = 0; k < bufSize; k++) {
        if (buf[k] == FLAG || buf[k] == ESC) {
            count += 1;
        }
    }

    unsigned int size = bufSize + 6 + count;
    unsigned char i[size];
    for (int k = 0; k < size; k++) {
        i[k] = 0;
    }
    // write info;
    i[0] = FLAG;
    i[1] = A;
    i[2] = CSET;
    i[3] = A ^ CSET;
    unsigned int bcc2 = 0, k = 0;
    for (int j = 0; j < bufSize; j++) {
        if (buf[j] == FLAG) {
            i[j + 4 + k] = ESC;
            i[j + 5 + k] = 0x5E;
            bcc2 ^= ESC;
            bcc2 ^= 0x5E;
            k++;

        } else if (buf[j] == ESC) {
            i[j + 4 + k] = ESC;
            i[j + 5 + k] = 0x5D;
            bcc2 ^= ESC;
            bcc2 ^= 0x5D;
            k++;

        } else {
            i[j + 4 + k] = buf[j];
            bcc2 ^= buf[j];
        }

    }
    i[size - 1] = FLAG;
    i[size - 2] = bcc2;


    while (alarmCount < nTries && !done) {


        if (alarmEnabled == FALSE) {
            state = START;
            if (write(fd, i, size) == -1) {
                printf("writing error\n");
                return -1;
            }
            alarm(timeout); // Set alarm to be triggered in 3s
            alarmEnabled = TRUE;
        }
        unsigned char received[1];
        int bytes = read(fd, received, 1);
        if (bytes == -1) {
            printf("reading error in llwrite\n");
            return -1;
        }
        //printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF));
        unsigned char a, c;
        switch (state) {
            case START:
                if (received[0] == FLAG) {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (received[0] == FLAG)
                    continue;
                else if (received[0] != A) {
                    state = START;
                } else {
                    a = received[0];
                    state = A_RCV;
                }
                break;
            case A_RCV:
                if (received[0] == FLAG) {
                    state = FLAG_RCV;
                } else if (received[0] == CRR) {
                    c = received[0];
                    state = C_RCV;
                } else if (received[0] == CREJ) {
                    if (write(fd, i, size) == -1) {
                        printf("writing error\n");
                        return -1;
                    }
                    state = START;
                } else {
                    state = START;
                }
                break;
            case C_RCV:
                if (received[0] != (a ^ c)) {
                    state = START;
                } else {
                    state = BCC_OK;
                }
                break;
            case BCC_OK:
                if (received[0] != FLAG) {
                    state = START;
                } else {
                    alarm(0);
                    alarmEnabled = FALSE;
                    done = TRUE;
                }
                break;
        }

    }

    return size;
}

////////////////////////////////////////////////
// LLREAD
////////////////////////////////////////////////
int llread(unsigned char *packet) {
    StateEnum state = START;
    int running = TRUE;
    int esc_activated = FALSE;
    int data_size = 0;
    unsigned char bcc2 = 0;
    while (running) {
        //Verifies if everything is okay with byte received
        unsigned char received[1];
        int byte = read(fd, received, 1);
        if (byte == -1) {
            printf("Reading error in llread\n");
            return -1;
        }
        //printf("var=0x%02X\n", (unsigned int)(received[0] & 0xFF)); // print do que recebe
        unsigned char a, c;
        switch (state) {
            case START:
                if (received[0] == FLAG) {
                    state = FLAG_RCV;
                }
                break;
            case FLAG_RCV:
                if (received[0] == FLAG)
                    continue;
                else if (received[0] != A) {
                    state = START;
                } else {
                    a = received[0];
                    state = A_RCV;
                }
                break;
            case A_RCV:
                if (received[0] == FLAG) {
                    state = FLAG_RCV;
                } else if (received[0] != CSET) {
                    state = START;
                } else {
                    c = received[0];
                    state = C_RCV;
                }
                break;
            case C_RCV:
                if (received[0] == FLAG) {
                    state = FLAG_RCV;
                } else if (received[0] != (a ^ c)) {
                    state = START;
                } else {
                    state = BCC_OK;
                }
                break;
            case BCC_OK:
                if (received[0] == FLAG) {
                    //printf("\nbcc1=0x%02X, unsigned char: %c\n", (unsigned int)(data[data_size-1] & 0xFF), data[data_size]);
                    if (packet[data_size - 1] == (bcc2 ^ packet[data_size - 1])) {
                        packet[data_size - 1] = 0;
                        data_size--;
                        running = FALSE;
                        unsigned char ack[] = {FLAG, A, CRR, (A ^ CRR), FLAG};
                        if (write(fd, ack, 5) == -1) {
                            printf("writing error\n");
                            return -1;
                        }

                    } else {
                        state = START;
                        data_size = 0;
                        bcc2 = 0;
                        unsigned char nack[] = {FLAG, A, CREJ, (A ^ CREJ), FLAG};
                        if (write(fd, nack, 5) == -1) {
                            printf("writing error\n");
                            return -1;
                        }
                    }
                } else {
                    if (data_size >= MAX_PAYLOAD_SIZE) {
                        unsigned char nack[] = {FLAG, A, CREJ, (A ^ CREJ), FLAG};
                        data_size = 0;
                        if (write(fd, nack, 5) == -1) {
                            printf("writing error\n");
                            return -1;
                        }
                        state = START;
                        bcc2 = 0;
                    }
                    if (received[0] == ESC) {
                        esc_activated = TRUE;
                        continue;
                    }
                    if (esc_activated) {
                        if (received[0] == 0x5E) {
                            packet[data_size] = FLAG;
                            data_size++;
                            bcc2 ^= ESC;
                            bcc2 ^= 0x5E;
                        }
                        if (received[0] == 0x5D) {
                            packet[data_size] = ESC;
                            data_size++;
                            bcc2 ^= ESC;
                            bcc2 ^= 0x5D;
                        }
                        esc_activated = FALSE;
                    } else {
                        packet[data_size] = received[0];
                        data_size++;
                        bcc2 ^= received[0];
                    }

                }

                break;

        }


    }
    return data_size;
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llclose(int showStatistics) {
    if (tcsetattr(fd, TCSANOW, &oldtio) == -1) {
        perror("tcsetattr");
        exit(-1);
    }

    close(fd);


    return 1;
}

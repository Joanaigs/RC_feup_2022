#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#define SIZE 200
char * input, *user, *pass, *host, *filePath, *fileName;

int sockfd, sockfd2;
#define SERVER_PORT 21

struct hostent *h;

int readResult(char string[], int size){
    FILE *fp = fdopen(sockfd, "r");
    do {
        memset(string, 0, size);
        string = fgets(string, size, fp);
        printf("%s", string);
    } while (string[3] != ' ');
    
}

int readFile(char string[], int size){
    FILE *file = fopen(fileName, "w");
    if (file== NULL){
        printf("opening file error\n");
        return -1;
    }
    char received[1024];
    int readSize;
    while((readSize = read(sockfd2, received, sizeof(received)))){
        for(int i=0; i<readSize; i++){
            printf("%c", received[i]);
        }
        if(readSize < 0){
            printf("reading from socket error\n");
            return -1;
        }
        if((readSize = fwrite(received, readSize, 1, file)) < 0){
            printf("writing in file error\n");
            return -1;
        }
    }
    fclose(file);
    char response[SIZE];
    size_t bytes = readResult(response, SIZE);
    if (response[0] != '2')
        return -1;
    
}

int divideInput(char * input){
    char *ftp = strtok(input, ":");
    if ((ftp == NULL) || (strcmp(ftp, "ftp") != 0)) {
        printf("It should be ftp\n");
        return -1;
    }
    char *  rest = strtok(NULL, "\0");
    int x = strlen(rest);
    char * temp = strtok(rest, ":");
    int y = strlen(temp);
    if(temp==NULL ||(temp[0]!='/')||(temp[1]!='/')||strlen(temp)<3){
        printf("Bad Input\n");
        return -1;
    }
    else if (x==y){
        user="anonymous";
        pass="";
        temp=&rest[2];
    }
    else{
        user=&temp[2];
        temp=strtok(NULL, "@");
        if ((temp == NULL) || strlen(temp) == 0) {
            printf("Bad input\n");
            return -1;
        }
        pass=temp;
        temp=strtok(NULL, "\0");
    }
    
    temp=strtok(temp, "/");
    if ((temp == NULL) || strlen(temp) == 0) {
        printf("Bad input\n");
        return -1;
    }
    host=temp;
    temp=strtok(NULL, "\0");
    if (temp == NULL) {
        printf("Bad input\n");
        return -1;
    }
    filePath=temp;
    fileName="a.txt";

    if (temp == NULL) {
        printf("Bad input\n");
        return -1;
    }
    char* last = strrchr(temp, '/');
    if (last != NULL) {
        fileName= last +1;
    }
    else {
        fileName=temp;
    }
    
    return 0;
}

int setUp(){
       if ((h = gethostbyname(host)) == NULL) {
        herror("gethostbyname()");
        exit(-1);
    }

    printf("Host name  : %s\n", h->h_name);
    printf("IP Address : %s\n", inet_ntoa(*((struct in_addr *) h->h_addr)));

    struct sockaddr_in server_addr;
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(inet_ntoa(*((struct in_addr *) h->h_addr)));    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(SERVER_PORT);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }
    char response[SIZE];
    if(!readResult(response, SIZE)||response[0]!='2'){
        printf("connection error\n");
        return -1;
    }

}

int login(){
    char commandUser[6+ strlen(user)];
    strcpy(commandUser, "user ");
    strcat(commandUser, user);
    strcat(commandUser, "\n");
    printf("%s", commandUser);
    size_t bytes = write(sockfd, commandUser, strlen(commandUser));
    if (bytes < 0 || bytes!=strlen(commandUser)){
        perror("write()");
        exit(-1);
    }
    char response[SIZE];
    bytes = readResult(response, SIZE);
    if(response[0]!='3'){
        printf("login user error\n");
        return -1;
    }


    char commandPass[6+ strlen(pass)];
    strcpy(commandPass, "pass ");
    strcat(commandPass, pass);
    strcat(commandPass, "\n");
    printf("%s", commandPass);
    bytes = write(sockfd, commandPass, strlen(commandPass));
    if (bytes < 0 || bytes!=strlen(commandPass)){
        perror("write()");
        exit(-1);
    }
    bytes = readResult(response, SIZE);
    if(response[0]!='2'){
        printf("login pass error\n");
        return -1;
    }
    return 0;
}

int pasv(){
    char cmdPasv[6+ strlen(filePath)];
    strcpy(cmdPasv, "pasv ");
    strcat(cmdPasv, "\n");
    printf("%s", cmdPasv);
    size_t bytes = write(sockfd, cmdPasv, strlen(cmdPasv));
    if (bytes < 0 || bytes!=strlen(cmdPasv)){
        perror("write()");
        exit(-1);
    }
    char response[SIZE];
    bytes = readResult(response, SIZE);
    if(response[0]!='2'){
        printf("passv error\n");
        return -1;
    }
    int ip1, ip2, ip3, ip4;
    int port1, port2;
    if ((sscanf(response, "227 Entering Passive Mode (%d,%d,%d,%d,%d,%d)", &ip1, &ip2, &ip3, &ip4, &port1, &port2)) < 0) {
            printf("passv port error\n");
            return -1;
    }
    char ip[SIZE];
    sprintf(ip, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    int port = port1 * 256 + port2;
    

    struct sockaddr_in server_addr;
    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(ip);    /*32 bit Internet address network byte ordered*/
    server_addr.sin_port = htons(port);        /*server TCP port must be network byte ordered */

    /*open a TCP socket*/
    if ((sockfd2 = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("socket()");
        exit(-1);
    }
    /*connect to the server*/
    if (connect(sockfd2,
                (struct sockaddr *) &server_addr,
                sizeof(server_addr)) < 0) {
        perror("connect()");
        exit(-1);
    }

}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s %s\n", argv[0], "ftp://[<user>:<password>@]<host>/<url-path>");
        return -1;
    }

    input=argv[1];
    if(divideInput(input)==-1){
        return -1;
    }
    
    printf("User: %s\n", user);
    printf("Password: %s\n", pass);
    printf("Host name: %s\n", host);
    printf("File path: %s\n", filePath);
    printf("File name: %s\n", fileName);

    if(setUp()==-1){
        return -1;
    }

    if(login()<0)
        return -1;

    if(pasv()<0)
        return -1;

    char cmdRetr[5+ strlen(filePath)];
    strcpy(cmdRetr, "retr ");
    strcat(cmdRetr, filePath);
    strcat(cmdRetr, "\n");
    printf("%s", cmdRetr);
    size_t bytes = write(sockfd, cmdRetr, strlen(cmdRetr));
    if (bytes < 0 || bytes!=strlen(cmdRetr)){
        perror("write()");
        exit(-1);
    }
    char response[SIZE];
    bytes = readResult(response, SIZE);
    if(response[0]!='1'){
        printf("retr error\n");
        return -1;
    }
    bytes = readFile(response, SIZE);

    if (close(sockfd2)<0) {
        perror("close()");
        exit(-1);
    }

    
    if (close(sockfd)<0) {
        perror("close()");
        exit(-1);
    }

    

    return 0;
}
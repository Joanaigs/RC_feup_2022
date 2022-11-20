#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>

char * input, *user, *pass, *host, *filePath;
int sockfd;
#define SERVER_PORT 21
struct hostent *h;
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
    filePath=strtok(NULL, "\0");
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
    char buf[] = "Mensagem de teste na travessia da pilha TCP/IP\n";
    size_t bytes;

    /*server address handling*/
    bzero((char *) &server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = inet_addr(h->h_addr);    /*32 bit Internet address network byte ordered*/
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

}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s %s\n", argv[0], "ftp://[<user>:<password>@]<host>/<url-path>");
        return -1;
    }
    int sockfd;
    struct sockaddr_in server_addr;

    input=argv[1];
    if(divideInput(input)==-1){
        return -1;
    }
    
    printf("User: %s\n", user);
    printf("Password: %s\n", pass);
    printf("Host name: %s\n", host);
    printf("File path: %s\n", filePath);

    if(setUp()==-1){
        return -1;
    }

    return 0;
}
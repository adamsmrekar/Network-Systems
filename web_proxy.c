//--------------------------------------------------------------------------
// Name: web_proxy.c 
// Author: Adam Smrekar
// Date: November 22, 2019
// Description: HTTP-based GET web proxy that handles multiple 
//              simulatanous requests from users. 
//              Note: Some of this code is referenced from boiler plate
//              code given for this assignment.
// usage: webproxy <port>
//--------------------------------------------------------------------------

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>      
#include <strings.h>     
#include <unistd.h>      
#include <sys/socket.h>  
#include <sys/types.h>
#include <linux/limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define MAXLINE  8192  // max text line length 
#define MAXBUF   8192  // max I/O buffer size 
#define LISTENQ  1024  // second argument to listen() 
#define ERROR400 "HTTP/1.1 200 Document Follows\r\nContent-Type:text/html\r\nContent-Length:34\r\n\r\n<html><h1>HTTP 400 Bad Request</h1>"

//--------------------------------------------------------------------------
// @Name: http_parse_host_name
// @Description:
//      Parses host name
//--------------------------------------------------------------------------

void http_parse_host_name(char *buf, char *hostname) {
    const char host_buf[] = "Host: ";
    char tmp_buf[MAXBUF] = { 0 };
    char *str;
    memcpy(tmp_buf, buf, MAXBUF);

    // Parsing Host Name
    if((str = strstr(tmp_buf, host_buf)) != NULL) {
        char host_name[MAXBUF];
        strtok(str, "\r\n");
        snprintf(host_name, MAXBUF, "%s", str + sizeof(host_buf) - 1);

        // Print
        printf("host_name: %s\n", host_name);

        memcpy(hostname, host_name, sizeof(host_name));
    }
    else {
        // https://stackoverflow.com/questions/41286260/parse-http-request-line-in-c
        // Referenced from stackoverflow user: Sanchke Dellowar
        // Used to parse test
        // Parse request
        const char *start_of_host = strchr(buf, ' ') + 1;
        const char *end_of_query;
        // Check if port specified
        if(strstr(start_of_host, ":") != NULL)
            end_of_query = strchr(start_of_host, ':');
        else
            end_of_query = strchr(start_of_host, ' ');

        // Initialize host_name with correct size of host
        char host_name[end_of_query - start_of_host];

        // Copy host name
        strncpy(host_name, start_of_host, end_of_query - start_of_host);

        // Add NULL terminator
        host_name[sizeof(host_name)] = 0;

        // Print
        printf("host_name: %s\n", host_name);

        memcpy(hostname, host_name, sizeof(host_name));

        printf("hostname: %s\n", hostname);
    }
    
}

//--------------------------------------------------------------------------
// @Name: http_parse_port
// @Description:
//      Parses port of the address
//--------------------------------------------------------------------------

int http_parse_port(char *buf) {
    const char host_buf[] = "Host: ";
    char tmp_buf[MAXBUF] = { 0 };
    char port[MAXBUF] = { 0 };
    char *str;
    int port_ret = 80;
    memcpy(tmp_buf, buf, MAXBUF);

    // Parsing Host Name
    if((str = strstr(tmp_buf, host_buf)) != NULL) {
        char host_name[MAXBUF];
        strtok(str, "\r\n");
        snprintf(host_name, MAXBUF, "%s", str + sizeof(host_buf) - 1);

        // Print
        printf("host_name: %s\n", host_name);

        // Parse Port
        memset(str, 0, sizeof(str));
        if((str = strstr(host_name, ":")) != NULL) {
            strtok(str, ":");
            snprintf(port, MAXBUF, "%s", str + 1);
            printf("Port: %s\n", port);
            port_ret = atoi(port);
            if(port_ret >= 1 && port_ret <= 65535) 
                printf("Port in range!\n");
            else 
                port_ret = 80;
        }
    }
    else {
        // Used to parse test
        // Parse request
        const char *start_of_host = strchr(buf, ' ') + 1;
        char *end_of_host;
        // Check if port specified
        if((str = strstr(start_of_host, ":")) != NULL) {
            end_of_host = strchr(start_of_host, ':');
            str = strtok(end_of_host, " ");
            snprintf(port, MAXBUF, "%s", str + 1);
            printf("Port: %s\n", port);
            port_ret = atoi(port);
            if(port_ret >= 1 && port_ret <= 65535) 
                printf("Port in range!\n");
            else 
                port_ret = 80;
        }
    }

    return port_ret;
}

//--------------------------------------------------------------------------
// @Name: http_valid_check
// @Description:
//      Checks of HTTP is valid
//--------------------------------------------------------------------------

int http_valid_check(char *buf) {
    char tmp_buf[MAXBUF] = { 0 };
    strcpy(tmp_buf, buf);

    // Check GET header
    if(strtok(tmp_buf, "\r\n") == NULL) {
        if( (strstr(tmp_buf, "GET")      == NULL) || 
            (strstr(tmp_buf, "/")        == NULL) || 
           ((strstr(tmp_buf, "HTTP/1.1") == NULL) && 
            (strstr(tmp_buf, "HTTP/1.0") == NULL)))
            return false;
        return false;   
    }

    return true;
}

//--------------------------------------------------------------------------
// @Name: http_if_host_exists
// @Description:
//      Checks if host exists
//--------------------------------------------------------------------------

int http_if_host_exists(char *buf) {
    char tmp_buf[MAXBUF] = { 0 };
    char host_name[MAXBUF] = { 0 };
    struct hostent *host;

    memcpy(tmp_buf, buf, MAXBUF);
    
    http_parse_host_name(tmp_buf, host_name);

    if((host = gethostbyname(host_name)) == NULL) 
        return false; 

    return true;
}

//--------------------------------------------------------------------------
// @Name: open_listenfd
// @Description:
//      Creates TCP socket for listening
//--------------------------------------------------------------------------

int open_listenfd(int port) {
    int listenfd, optval = 1;
    struct sockaddr_in serveraddr;

    // Create a socket descriptor 
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        return -1;

    // Eliminates "Address already in use" error from bind. 
    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, 
                   (const void *)&optval , sizeof(int)) < 0)
        return -1;

    // listenfd will be an endpoint for all requests to port
    // on any IP address for this host 
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET; 
    serveraddr.sin_addr.s_addr = htonl(INADDR_ANY); 
    serveraddr.sin_port = htons((unsigned short)port); 
    if (bind(listenfd, (struct sockaddr*)&serveraddr, sizeof(serveraddr)) < 0)
        return -1;

    // Make it a listening socket ready to accept connection requests 
    if (listen(listenfd, LISTENQ) < 0)
        return -1;
    return listenfd;
} 

//--------------------------------------------------------------------------
// @Name: connectfd
// @Description:
//      
//--------------------------------------------------------------------------

int connectfd(int port, char *host_name) {
    struct addrinfo hints, *res;
    int sockfd;
    char port_c[6] = { 0 };
    
    // Convert port int to string
    snprintf(port_c, 6, "%d", port);

    // Load up address structs with getaddrinfo()
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    getaddrinfo(host_name, port_c, &hints, &res);

    // Create a socket descriptor 
    if ((sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0)
        return -1;

    // Connect
    if (connect(sockfd, res->ai_addr, res->ai_addrlen) < 0)
        return -1;

    return sockfd;
} 

//--------------------------------------------------------------------------
// @Name: handle_requests
// @Description:
//      
//--------------------------------------------------------------------------

void handle_requests(int connfd) {
    size_t n; 
    int r, file_found, sockfd;
    int host_exists = 0;
    int valid = 0;
    int port = 0;
    int read_size = 0;
    long file_size;
    char content_type[MAXBUF] = { 0 };
    char host_name[MAXBUF] = { 0 };
    char buf[MAXLINE] = { 0 }; 
    char recv_buf[MAXLINE] = { 0 };
    FILE *fp;

    // Clear Buffer
    memset(buf, 0, MAXBUF);

    // Read input
    if(read(connfd, buf, MAXLINE) < 0)
        printf("Error with read 1\n");

    // Check if http method is valid
    valid = http_valid_check(buf);
    if(valid) {
        printf("HTTP Valid!\n");
        
        // Check if requested IP exists
        host_exists = http_if_host_exists(buf);
    }
    else
        printf("HTTP Invalid!\n");

    
    if(host_exists) {
        // Make TCP connection with server or client
        // Depending on which side of the proxy
        http_parse_host_name(buf, host_name);
        printf("Host [%s] exists!\n", host_name);

        // Get port number (if not specified, port = 80)
        port = http_parse_port(buf);
        printf("Port: %d\n", port);

        // Connect to host_name
        sockfd = connectfd(port, host_name);
        printf("Sockfd: %d\n", sockfd);

        // Send request to host_name
        if(write(sockfd, buf, MAXBUF) < 0)
            printf("Error with send to host\n");

        // Read request from host_name
        // Send to client
        while((read_size = read(sockfd, buf, MAXBUF)) > 0) {
            write(connfd, buf, read_size);
            bzero(buf, MAXBUF);
        }
        close(sockfd);
    }
    else {
        http_parse_host_name(buf, host_name);
        printf("Host [%s] doesn't exist!\n", host_name);
        write(connfd, ERROR400, sizeof(ERROR400));
    }
    
}

//--------------------------------------------------------------------------
// @Name: thread
// @Description:
//      Thread worker. Detaches thread and calls handle_requests function
//--------------------------------------------------------------------------

void *thread(void *vargp) {  
    int connfd = *((int *)vargp);
    pthread_detach(pthread_self()); 
    free(vargp);
    handle_requests(connfd);
    close(connfd);
    return NULL;
}

//--------------------------------------------------------------------------
//--------------------------------------------------------------------------

int main(int argc, char **argv) {
    int listenfd, *connfdp, port;
    int clientlen = sizeof(struct sockaddr_in);
    struct sockaddr_in clientaddr;
    pthread_t tid; 

    if (argc != 2) {
        fprintf(stderr, "usage: webproxy <port>\n");
        exit(0);
    }
    port = atoi(argv[1]);

    listenfd = open_listenfd(port);
    while (1) {
        connfdp = malloc(sizeof(int));
        *connfdp = accept(listenfd, (struct sockaddr*)&clientaddr, &clientlen);
        pthread_create(&tid, NULL, thread, connfdp);
    }
}
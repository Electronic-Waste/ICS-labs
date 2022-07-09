/*
 * proxy.c - ICS Web proxy
 *
 *
 */

#include "csapp.h"
#include <stdarg.h>
#include <string.h>
#include <sys/select.h>
#include <pthread.h>

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, char *port);

void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, size_t size);

void doit(int fd, SA *sa);

void *doit_one_thread(void *vargp);

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n)
{
    ssize_t size;
    
    if ((size = rio_readnb(rp, usrbuf, n)) < 0) {
        fprintf(stderr, "An error occurs in Rio_readnb_w!\n");
        return -1;
    }

    return size;
}

ssize_t Rio_readlineb_w(rio_t *rp, void *usrbuf, size_t maxlen)
{
    ssize_t size;

    if ((size = rio_readlineb(rp, usrbuf, maxlen)) < 0) {
        fprintf(stderr, "An error occurs in Rio_readlineb_w\n");
        return -1;
    }

    return size;
}

ssize_t Rio_writen_w(int fd, void *usrbuf, size_t n)
{
    ssize_t size;

    if ((size = rio_writen(fd, usrbuf, n)) < 0) {
        fprintf(stderr, "An error occurs in Rio_writen_w\n");
        return -1;
    }
    return size;
}

typedef struct info {
    int *connfd;
    SA *clientaddr;
} info_t;

/*
 * main - Main routine for the proxy program
 */
int main(int argc, char **argv)
{
    /* Check arguments */
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
        exit(0);
    }

    int listenfd;
    int *connfd;
    socklen_t clientlen;
    struct sockaddr_storage *clientaddr;
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;
    info_t *info;

    listenfd = Open_listenfd(argv[1]);
    while (1) {
        clientlen = sizeof(clientaddr);
        clientaddr = Malloc(sizeof(struct sockaddr_storage));
        connfd = Malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) clientaddr, &clientlen);
        info_t *info = Malloc(sizeof(info_t));
        info->connfd = connfd;
        info->clientaddr = clientaddr;
        Pthread_create(&tid, NULL, doit_one_thread, info);
        // Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        // printf("Connected to (%s %s)\n", client_hostname, client_port);
        // doit(*connfd, (SA *) clientaddr);
        // Close(connfd);
    }

    exit(0);
}

void *doit_one_thread(void *vargp)
{
    info_t *info = (info_t *) vargp;
    int connfd = *info->connfd;
    SA clientaddr = *info->clientaddr;
    Pthread_detach(pthread_self());
    Free(vargp);
    doit(connfd, (SA *) &clientaddr);
    Close(connfd);
    return NULL;

}

void doit(int fd, SA *sa) 
{
    int serverfd;           // The fd connected to server
    ssize_t nread;          // Buf size read from server
    ssize_t totalLength = 0;    // Content length of response from server
    ssize_t requestBodyLen;     // The length of request body (if exists)
    ssize_t responseBodyLen;
    char method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char hostname[MAXLINE], pathname[MAXLINE], port[MAXLINE];
    char req_from_client[MAXLINE] = {'\0'}, req_to_server[MAXLINE] = {'\0'}; 
    char respon_from_server[MAXLINE] = {'\0'}, respon_to_client[MAXLINE] = {'\0'};
    char longstring[MAXLINE];
    rio_t rio, server_rio;

    /* Read request line from the client */
    Rio_readinitb(&rio, fd);
    if (Rio_readlineb_w(&rio, req_from_client, MAXLINE) <= 0) {
        fprintf(stderr, "read error: invalid request line from client\n");
        return;
    }
    /* Get method, uri, version */
    if (sscanf(req_from_client, "%s %s %s", method, uri, version) != 3) {
        fprintf(stderr, "sscanf error: arguments number is invalid\n");
        return;
    } 
    /* Get hostname, pathname and port from uri */
    if (parse_uri(uri, hostname, pathname, port) == -1) {
        fprintf(stderr, "parse error: invalid uri");
        return;
    }      

    /* Parse request line and send it to server */
    serverfd = Open_clientfd(hostname, port);
    sprintf(req_to_server, "%s /%s %s\r\n", method, pathname, version); // Make up request line for server
    Rio_writen_w(serverfd, req_to_server, strlen(req_to_server));
    memset(req_to_server, 0, MAXLINE);

    /* Read request header from the client and send it to server */
    while (strcasecmp(req_from_client, "\r\n") != 0 
        && (nread = Rio_readlineb_w(&rio, req_from_client, MAXLINE)) != 0) {
        if (strncasecmp(req_from_client, "Content-Length", 14) == 0) {
            requestBodyLen = strtol(req_from_client + 16, NULL, 10);
        }
        sprintf(req_to_server, "%s%s", req_to_server, req_from_client);
        // Rio_writen_w(serverfd, req_from_client, MAXLINE);
    }
    if (nread == 0) {
        fprintf(stderr, "Wrong request header from client\n");
        close(serverfd);
        return;
    }
    Rio_writen_w(serverfd, req_to_server, strlen(req_to_server));
    memset(req_from_client, 0, MAXLINE);

    /* If there exists request body */
    if (strcmp(method, "GET")) {
        for (int i = 0; i < requestBodyLen; ++i) {
            if (Rio_readnb_w(&rio, req_from_client, 1) == 0) break;
            Rio_writen_w(serverfd, req_from_client, 1);
        }
    }

    /* Read response header */
    Rio_readinitb(&server_rio, serverfd);
    while (strcasecmp(respon_from_server, "\r\n") != 0 
        && (nread = Rio_readlineb_w(&server_rio, respon_from_server, MAXLINE)) != 0){
        if (strncasecmp(respon_from_server, "Content-Length", 14) == 0) {
            responseBodyLen = strtol(respon_from_server + 16, NULL, 10);
            totalLength += responseBodyLen;
        }
        totalLength += strlen(respon_from_server);
        sprintf(respon_to_client, "%s%s", respon_to_client, respon_from_server);
        // Rio_writen_w(fd, respon_from_server, MAXLINE);
    }
    if (nread == 0) {
        fprintf(stderr, "Wrong response header from server\n");
        close(serverfd);
        return;
    }
    Rio_writen_w(fd, respon_to_client, strlen(respon_to_client));
    memset(respon_to_client, 0 , MAXLINE);

    format_log_entry(longstring, sa, uri, totalLength);
    printf("%s\n", longstring);

    /* Read response body and send it to client */
    while (Rio_readnb_w(&server_rio, respon_to_client, 1) > 0) {
        Rio_writen_w(fd, respon_to_client, 1);
        responseBodyLen--;
    }
    if (responseBodyLen > 0) {
        fprintf(stderr, "Wrong response body from server\n");
        close(serverfd);
        return;
    }

    Close(serverfd);
    return;
    
}


/*
 * parse_uri - URI parser
 *
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, char *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
        hostname[0] = '\0';
        return -1;
    }

    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    if (hostend == NULL)
        return -1;
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';

    /* Extract the port number */
    if (*hostend == ':') {
        char *p = hostend + 1;
        while (isdigit(*p))
            *port++ = *p++;
        *port = '\0';
    } else {
        strcpy(port, "80");
    }

    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
        pathname[0] = '\0';
    }
    else {
        pathbegin++;
        strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring.
 *
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), the number of bytes
 * from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr,
                      char *uri, size_t size)
{
    time_t now;
    char time_str[MAXLINE];
    char host[INET_ADDRSTRLEN];

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    if (inet_ntop(AF_INET, &sockaddr->sin_addr, host, sizeof(host)) == NULL)
        unix_error("Convert sockaddr_in to string representation failed\n");

    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %s %s %zu", time_str, host, uri, size);
}

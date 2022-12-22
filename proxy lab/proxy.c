#include <stdio.h>
#include "csapp.h"

void do_proxy(int fd);
void read_requesthdrs(rio_t *rp);
void send_requesthdrs(int clientfd, char* method, char* query, char* hostname);
void parse_uri(char* uri, char* hostname, char* query, int *port);
void response(int proxfd,int clientfd);

void *thread_routine(void *argp);
void *pre_thread_routine(void *argp);

/* Recommended max cache sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define SBUFSIZE 20
#define NTHREADS 10

/* From sbuf.h and sbuf.c from csapp code */
typedef struct {
    int *buf;          /* Buffer array */         
    int n;             /* Maximum number of slots */
    int front;         /* buf[(front+1)%n] is first item */
    int rear;          /* buf[rear%n] is last item */
    sem_t mutex;       /* Protects accesses to buf */
    sem_t slots;       /* Counts available slots */
    sem_t items;       /* Counts available items */
} sbuf_t;
void sbuf_init(sbuf_t *sp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);
sbuf_t sbuf;

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

int main(int argc,char ** argv)
{   
    if(argc != 2){
        fprintf(stderr,"usage :%s <port> \n",argv[0]);
        exit(1);
    }

    int listenfd, connfd;
    char hostname[MAXLINE], port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    listenfd = Open_listenfd(argv[1]);
    int *connfdp;
    pthread_t tid;
    
    /* Sequential Model */ 
    //  while(1){
    //     clientlen = sizeof(clientaddr);
    //     connfd = Accept(listenfd,(SA*)&clientaddr,&clientlen);
    //     Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
    //     printf("Accepted connection from (%s, %s)\n",hostname,port);

    //     do_proxy(connfd);
    //     Close(connfd);
    // }

    /* Concurrent Model */
    // while(1){
    //     clientlen = sizeof(clientaddr);
    //     connfdp = Malloc(sizeof(int));
    //     *connfdp = Accept(listenfd,(SA*)&clientaddr,&clientlen);
    //     Getnameinfo((SA*)&clientaddr,clientlen,hostname,MAXLINE,port,MAXLINE,0);
    //     printf("Accepted connection from (%s, %s)\n",hostname,port);

    //     Pthread_create(&tid, NULL, thread_routine, connfdp);
    // }

    /* Prethreaded Concurrent Model */
    sbuf_init(&sbuf, SBUFSIZE);
    for(int i=0;i<NTHREADS;i++)
        Pthread_create(&tid,NULL,pre_thread_routine,NULL);

    while(1){
        clientlen = sizeof(clientaddr);
        connfd = Accept(listenfd,(SA*)&clientaddr,&clientlen);
        sbuf_insert(&sbuf, connfd);
    }

    return 0;
}

void do_proxy(int fd){
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE], hostname[MAXLINE], port_[MAXLINE], query[MAXLINE];
    rio_t rio;
    int port;

    Rio_readinitb(&rio, fd);
    if(!Rio_readlineb(&rio,buf,MAXLINE)) return;
    printf("Request line: %s" ,buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if(strcmp(method,"GET")){
        printf("only GET method implmented");
        return;
    }
    parse_uri(uri,hostname,query,&port);
    sprintf(port_,"%d",port);
    printf("%s\n", port_);
    int proxfd = Open_clientfd(hostname,port_);
    
    send_requesthdrs(proxfd, method, query, hostname);
    response(proxfd,fd);
    Close(proxfd);

    return;
}

void read_requesthdrs(rio_t *rp){
    char buf[MAXLINE];

    Rio_readlineb(rp,buf,MAXLINE);
    while(strcmp(buf,"\r\n")){
        Rio_readlineb(rp,buf,MAXLINE);
        printf("%s", buf);
    }
    return;
}

void send_requesthdrs(int proxfd, char* method, char* query, char* hostname){

    char buf[MAXLINE];
    sprintf(buf, "%s %s HTTP/1.0\r\n", method, query);
    sprintf(buf, "%sHost: %s\r\n", buf, hostname);
    strcat(buf, user_agent_hdr);
    strcat(buf, "Connection: close\r\n");
    strcat(buf, "Proxy-Connection: close\r\n\r\n");
    Rio_writen(proxfd, buf, strlen(buf));

    printf("%s", buf);
}

void parse_uri(char* uri, char* hostname, char* query, int *port){

    char* hostpos;
    char* portpos;
    char* querypos;

    *port = 80;
    hostpos = strstr(uri,"//") ? strstr(uri,"//")+2 : uri;
    portpos = strstr(hostpos,":");
    if(portpos){
        *portpos = '\0';
        sscanf(hostpos, "%s", hostname);
        sscanf(portpos+1, "%d%s", port, query);
    }else{
        querypos = strstr(hostpos, "/");
        if(querypos){
            *querypos = '\0';
            sscanf(hostpos,"%s", hostname);
            *querypos = '/';
            sscanf(querypos,"%s", query);
        }else{
            sscanf(hostpos, "%s", hostname);
        }
    }
    return;
}

void response(int proxfd,int clientfd){
    rio_t rio;
    int n;
    char buf[MAXLINE];
    Rio_readinitb(&rio, proxfd);
    while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
        Rio_writen(clientfd, buf, n);
    }
}

void *thread_routine(void *argp){
    int connfd = *((int *)argp);
    Pthread_detach(Pthread_self());
    Free(argp);

    do_proxy(connfd);
    Close(connfd);
    return NULL;
}
void *pre_thread_routine(void *argp){
    Pthread_detach(Pthread_self());
    while (1){
        int connfd = sbuf_remove(&sbuf);
        do_proxy(connfd);
        Close(connfd);
    }
}

void sbuf_init(sbuf_t *sp, int n)
{
    sp->buf = Calloc(n, sizeof(int)); 
    sp->n = n;                       /* Buffer holds max of n items */
    sp->front = sp->rear = 0;        /* Empty buffer iff front == rear */
    Sem_init(&sp->mutex, 0, 1);      /* Binary semaphore for locking */
    Sem_init(&sp->slots, 0, n);      /* Initially, buf has n empty slots */
    Sem_init(&sp->items, 0, 0);      /* Initially, buf has zero data items */
}
void sbuf_deinit(sbuf_t *sp)
{
    Free(sp->buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp->slots);                          /* Wait for available slot */
    P(&sp->mutex);                          /* Lock the buffer */
    sp->buf[(++sp->rear)%(sp->n)] = item;   /* Insert the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->items);                          /* Announce available item */
}
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp->items);                          /* Wait for available item */
    P(&sp->mutex);                          /* Lock the buffer */
    item = sp->buf[(++sp->front)%(sp->n)];  /* Remove the item */
    V(&sp->mutex);                          /* Unlock the buffer */
    V(&sp->slots);                          /* Announce available slot */
    return item;
}
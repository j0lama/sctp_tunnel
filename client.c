#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include<pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/sctp.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>

#define BUFFER_LEN 1024

#define RED   "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW   "\x1B[33m"
#define BLUE   "\x1B[34m"
#define MAGENT   "\x1B[35m"
#define CYAN   "\x1B[36m"
#define WHITE   "\x1B[37m"
#define RESET "\x1B[0m"

/* Global decriptors to be accessed from threads */
int client, sock_sctp, sock;
/* Thread descriptors */
pthread_t downlink_id, uplink_id;

void sigint_handler(int dummy) {
    pthread_cancel(downlink_id);
    pthread_cancel(uplink_id);
}

void * downlink_thread()
{
    /* (RAN) <- (TCP Tunnel) */
    char buffer[BUFFER_LEN + sizeof(struct sctp_sndrcvinfo)];
    int len;
    struct sctp_sndrcvinfo sndrcvinfo;
    int struct_size = sizeof(sndrcvinfo);
    printf("%sDownlink Thread: Started correctly%s\n", GREEN, RESET);

    /* Forward all data from the TCP server to the SCTP client */
    while(1)
    {
        bzero(buffer, BUFFER_LEN + struct_size);
        /* Addapted to receive in the first struct_size bytes of the buffer a sctp_sndrcvinfo structure and forward it to the SCTP client*/
        len = (int)recv(sock, buffer, BUFFER_LEN + struct_size, 0);
        /* If connection is closed */
        if(len <= 0)
        {
            printf("Downlink thread: Connection closed\n");
            /* Exiting threads */
            pthread_cancel(uplink_id);
            pthread_exit(0);
        }
        memcpy(&sndrcvinfo, buffer, struct_size);
        sctp_send(client, (void *) buffer + struct_size, (size_t) len - struct_size, &sndrcvinfo, 0);
        printf("%s(RAN <- TCP Tunnel): %d bytes%s\n", GREEN, len - struct_size, RESET);
    }
}

void * uplink_thread()
{
    /* (RAN) -> (TCP Tunnel) */
    char buffer[BUFFER_LEN + sizeof(struct sctp_sndrcvinfo)];
    int len;
    struct sctp_sndrcvinfo sndrcvinfo;
    int struct_size = sizeof(sndrcvinfo);
    int flags;
    printf("%sUplink Thread: Started correctly%s\n", RED, RESET);

    /* Forward all data from the SCTP client to the TCP server */
    while(1)
    {
        bzero(buffer, BUFFER_LEN + struct_size);
        /* Addapted to send in the first struct_size bytes of the buffer a sctp_sndrcvinfo structure*/
        len = sctp_recvmsg(client, buffer + struct_size, BUFFER_LEN, (struct sockaddr *) NULL, 0, &sndrcvinfo, &flags);
        /* If connection is closed */
        if(len <= 0)
        {
            printf("Uplink thread: Connection closed\n");
            /* Exiting threads */
            pthread_cancel(downlink_id);
            pthread_exit(0);
        }
        memcpy(buffer, &sndrcvinfo, struct_size);
        send(sock, buffer, len + struct_size, 0);
        printf("%s(RAN -> TCP Tunnel): %d bytes%s\n", RED, len, RESET);
    }
}



int main(int argc, char const *argv[])
{
    int addrlen, bytes_readed, i;
    const char * tunnel_ip, * sctp_server_ip;
    int tunnel_port, sctp_server_port;
    struct sctp_event_subscribe events;
    struct sockaddr_in my_addr, remote_addr;
    struct sockaddr_in sctp_server_addr;



    if(argc != 5)
    {
        printf("USE: ./client <TUNNEL_SERVER_IP> <TUNNEL_SERVER_PORT> <SCTP_IP> <SCTP_PORT>\n");
        exit(1);
    }
    /* Getting parameters */
    tunnel_ip = argv[1];
    tunnel_port = atoi(argv[2]);
    sctp_server_ip = argv[3];
    sctp_server_port = atoi(argv[4]);

    /* Ctrl + c signal handler */
    signal(SIGINT, sigint_handler);

    /*
    * SCTP Server Set Up
    */
    sock_sctp = socket(PF_INET, SOCK_STREAM, IPPROTO_SCTP);
    if (sock_sctp < 0)
    {
        perror("sctp socket error");
        exit(1);
    }

    /* Sets the data_io_event to be able to use sendrecv_info */
    /* Subscribes to the SCTP_SHUTDOWN event, to handle graceful shutdown */
    bzero(&events, sizeof(events));
    events.sctp_data_io_event          = 1;
    events.sctp_shutdown_event         = 1;
    if (setsockopt(sock_sctp, IPPROTO_SCTP, SCTP_EVENTS, &events, sizeof(events)) != 0) {
        perror("setsockopt SCTP error");
        close(sock_sctp);
        exit(1);
    }

    /* Set up SCTP Server address */
    memset(&sctp_server_addr, 0, sizeof(sctp_server_addr));
    sctp_server_addr.sin_family = AF_INET;
    sctp_server_addr.sin_addr.s_addr = inet_addr(sctp_server_ip);
    sctp_server_addr.sin_port = htons(sctp_server_port);

    /*Binding*/
    if(bind(sock_sctp, (struct sockaddr *) &sctp_server_addr, sizeof(struct sockaddr)) == -1)
    {
        perror("bind");
        close(sock_sctp);
        exit(1);
    }

    /* Listening */
    if(listen(sock_sctp, SOMAXCONN))
    {
        perror("listen");
        close(sock_sctp);
        exit(1);
    }

    addrlen = sizeof(struct sockaddr);
    client = accept(sock_sctp, (struct sockaddr *)&remote_addr, &addrlen);
    printf("OK: SCTP Server client connected\n");
    /* SCTP Client connected */


    /*
    * Tunnel Socket Set Up
    */
    /* Creating TCP socket*/ 
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if(sock == -1)
    {
        perror("socket");
        exit(1);
    }

    /* Set up tunnel parameters */
    my_addr.sin_family = AF_INET;
    my_addr.sin_addr.s_addr = inet_addr(tunnel_ip);
    my_addr.sin_port = htons(tunnel_port);
    memset(&(my_addr.sin_zero), 0, 8);
    
    /* Connect to the SCTP server */
    if (connect(sock, (struct sockaddr*)&my_addr, sizeof(my_addr)))
    {
        perror("tcp connect error");
        close(client);
        close(sock);
        exit(1);
    }
    printf("OK: Tunnel client connected\n");
    /* TCP Tunnel established */


    /* Init both threads to handle downlink and uplink messages */
    pthread_create(&downlink_id, NULL, &downlink_thread, NULL);
    pthread_create(&uplink_id, NULL, &uplink_thread, NULL);

    /* Wait util thread finish */
    pthread_join(downlink_id, NULL);
    pthread_join(uplink_id, NULL);

    printf("\nClosing decriptors...\n");
    close(sock_sctp);
    close(sock);
    close(client);


    printf("Done!\n");

    return 0;
}
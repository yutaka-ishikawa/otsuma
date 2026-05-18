/*
 * This eample code might run OK, but contains a wrong system call handling
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>

#define TCP_PORT	1219

int	port = TCP_PORT;

void
server(char *peer, int flag)
{
    struct hostent	*hp;
    struct sockaddr_in	saddr_in;
    struct sockaddr_in	saddr_out;
    socklen_t		addrlen;
    int			sock;
    int			fd;
    char		buf[512];

    bzero((char*)&saddr_in, sizeof(saddr_in));
    if (!strcmp(peer, "any")) {
	saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons(port);
	printf("receiving from any host\n");
    } else {
	if ((hp = gethostbyname(peer)) == NULL) {
	    fprintf(stderr, "Canot obtain the %s host info\n", peer);
	    exit(-1);
	}
	bcopy(hp->h_addr, (char *)&saddr_in.sin_addr, hp->h_length);
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons(port);
	printf("receiving from %s\n", peer);
    }
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "Cannot open a SOCK_DGRAM socket\n");
	exit(-1);
    }
    if (bind(sock, (struct sockaddr*) &saddr_in, sizeof(saddr_in)) < 0) {
	fprintf(stderr, "Cannot bind\n");
	exit(-1);
    }

    if (listen(sock, 10) < 0) {
	fprintf(stderr, "Listen error\n");
	exit(-1);
    }
    addrlen = sizeof(saddr_out);
    if ((fd = accept(sock, (struct sockaddr*) &saddr_out, &addrlen)) < 0) {
	fprintf(stderr, "Error\n");
	exit(-1);
    }
    printf("fd = %d sock = %d\n", fd, sock);
    while(read(fd, buf, 512) > 0) {
	printf("buf = %s\n", buf);
    }
}

void
client(char *host, char *msg)
{
    struct hostent	*hp;
    struct sockaddr_in	saddr_in;
    int			sock;

    bzero((char*)&saddr_in, sizeof(saddr_in));

    if ((hp = gethostbyname(host)) == NULL) {
	fprintf(stderr, "Canot obtain the host info\n");
	exit(-1);
    }
    bcopy(hp->h_addr, (char *)&saddr_in.sin_addr, hp->h_length);
    saddr_in.sin_family = AF_INET;
    saddr_in.sin_port = htons(port);
    if ((sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
	fprintf(stderr, "Cannot open a SOCK_DGRAM socket\n");
	exit(-1);
    }

    if (connect(sock, (struct sockaddr*) &saddr_in, sizeof(saddr_in)) < 0) {
        fprintf(stderr, "Client:Can't connect Inet socket.\n");
        close(sock);
        exit(-1) ;
    }
    write(sock, msg, strlen(msg) + 1);
}

void
usage()
{
    fprintf(stderr, "./a.out server sender-host-name|any [verbos]\n");
    fprintf(stderr, "./a.out client server-host-name message\n");
    exit(-1);
}

int
main(int argc, char **argv)
{
    if (argc < 3) usage();
    {
	char	*cp = getenv("TCP_PORT");
	if (cp) {
	    port = atoi(cp);
	}
	printf("tcp port = %d\n", port);
    }
    if (!strcmp(argv[1], "server")) {
	server(argv[2], argc >= 4);
    } else if (!strcmp(argv[1], "client")) {
	client(argv[2], argv[3]);
    } else usage();
    return 0;
}


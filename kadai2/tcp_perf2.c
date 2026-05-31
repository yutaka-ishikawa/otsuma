/*
 */
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <time.h>

#define TCP_PORT	1219
#define COM_SIZE	(256*1024*1024)
//#define MAX_DSIZE	(1024*1024)
#define MAX_DSIZE	(256*1024*1024)
#define MAX_TENT	30	/* log2 (1024*1024*1024) */
//#define ITER		100
#define ITER		10

int	port = TCP_PORT;
char	combuf[COM_SIZE];
double	tcost[MAX_TENT];

static void
init_data()
{
    int	i;
    for (i = 0; i < MAX_DSIZE; i++) {
	combuf[i] = i;
    }
}


static inline double
latency(struct timespec *ts_start, struct timespec *ts_end, int iter)
{
    double sec = ts_end->tv_sec - ts_start->tv_sec;
    double nsec = ts_end->tv_nsec - ts_start->tv_nsec;
    double usec;
    usec = ((double)sec*1000000 + (double)nsec/1000);
    usec /= (double)iter;
    return usec;
}

/*
 * One way latency is calculated as RTT (Round Trip Time ) / 2
 * tcost[] keeps microsecond time
 */
static void
show_throughput()
{
    ssize_t	sz;
    int		ent = 0;
    printf("size,MByte/sec,usec\n");
    for (sz = 1; sz <= MAX_DSIZE; sz = sz << 1) {
	printf("%ld,%.3e,%.3e\n", sz, ((double)sz)/(tcost[ent]/2.0), tcost[ent]);
	ent++;
    }
}

int
reader(int fd, ssize_t rsize)
{
    ssize_t	off = 0;
    int	rc = 0;
    do {
	ssize_t	rz;
	rz = read(fd, combuf + off, rsize);
	if (rz < 0) {
	    perror("read");
	    rc = -1;
	    goto err_exit;
	} else if (rz == 0) { /* peer process closes the connection */
	    break;
	}
	rsize -= rz;
	off += rz;
    } while (rsize > 0);
err_exit:
    return rc;
}

int
writer(int fd, ssize_t wsize)
{
    ssize_t	off = 0;
    int	rc = 0;
    do {
	ssize_t	wz;
	wz = write(fd, combuf + off, wsize);
	if (wz == 0 || wz < 0) {
	    perror("write");
	    rc = -1;
	    goto err_exit;
	}
	wsize -= wz;
	off += wz;
    } while (wsize > 0);
err_exit:
    return rc;
}

static inline int
read_write(int fd, ssize_t size, int iter)
{
    int	rc;

    for (iter = 0; iter < ITER; iter++) {
	rc = reader(fd, size); if (rc < 0) goto err_exit;
	rc = writer(fd, size); if (rc < 0) goto err_exit;
    }
err_exit:
    return rc;
}

static int inline
write_read(int fd, ssize_t size, int iter)
{
    int	rc;

    for (iter = 0; iter < ITER; iter++) {
	rc = writer(fd, size); if (rc < 0) goto err_exit;
	rc = reader(fd, size); if (rc < 0) goto err_exit;
    }
err_exit:
    return rc;
}

static void
server(char *peer, int flag)
{
    struct hostent	*hp;
    struct sockaddr_in	saddr_in;
    struct sockaddr_in	saddr_out;
    socklen_t		addrlen;
    int			sock;
    int			fd;
    int			rc = 0;

    bzero((char*)&saddr_in, sizeof(saddr_in));
    if (!strcmp(peer, "any")) {
	saddr_in.sin_addr.s_addr = htonl(INADDR_ANY);
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons(port);
	fprintf(stderr, "receiving from any host\n");
    } else {
	if ((hp = gethostbyname(peer)) == NULL) {
	    fprintf(stderr, "Canot obtain the %s host info\n", peer);
	    exit(-1);
	}
	bcopy(hp->h_addr, (char *)&saddr_in.sin_addr, hp->h_length);
	saddr_in.sin_family = AF_INET;
	saddr_in.sin_port = htons(port);
	fprintf(stderr, "receiving from %s\n", peer);
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
    fprintf(stderr, "fd = %d sock = %d\n", fd, sock);
    
    {
	ssize_t	rsize;
	for (rsize = 1; rsize <= MAX_DSIZE; rsize = rsize << 1) {
	    rc = read_write(fd, rsize, ITER);
	    if (rc < 0) goto err_exit;
	}
    }
err_exit:
    return;
}

static void
client(char *host)
{
    struct hostent	*hp;
    struct sockaddr_in	saddr_in;
    int			sock;
    struct timespec	st, et;
    int	rc = 0;

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
    {
	ssize_t	wsize;
	int	ent = 0;
	for (wsize = 1; wsize <= MAX_DSIZE; wsize = wsize << 1) {
	    clock_gettime(CLOCK_MONOTONIC, &st);
	    rc = write_read(sock, wsize, ITER); if (rc < 0) goto err_exit;
	    clock_gettime(CLOCK_MONOTONIC, &et);
	    tcost[ent++] = latency(&st, &et, ITER);
	}
    }
err_exit:
    return;
}

void
usage(char *cmd)
{
    fprintf(stderr, "%s server sender-host-name|any [verbos]\n", cmd);
    fprintf(stderr, "%s client server-host-name\n", cmd);
    exit(-1);
}

int
main(int argc, char **argv)
{
    if (argc < 3) usage(argv[0]);
    {
	char	*cp = getenv("TCP_PORT");
	if (cp) {
	    port = atoi(cp);
	}
	fprintf(stderr, "tcp port = %d\n", port);
    }
    init_data();
    if (!strcmp(argv[1], "server")) {
	server(argv[2], argc >= 4);
    } else if (!strcmp(argv[1], "client")) {
	client(argv[2]);
	show_throughput();
    } else usage(argv[0]);
    return 0;
}

/*
 * simple_httpd.c
 *
 * Build:
 *      cc -Wall -O2 -o simple_httpd simple_httpd.c
 * Run:
 *      ./simple_httpd 8080
 * Test:
 *	URL   http://127.0.0.1:8080
 *
 *      curl http://127.0.0.1:8080/
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define DEFAULT_PORT	8080
#define REQ_SIZE	4096
#define BUF_SIZE	256

static void
usage(prog)
char *prog;
{
    fprintf(stderr, "Usage: %s [port]\n", prog);
}


static int
open_listen_socket(int port)
{
    int fd;
    int on;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    on = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR,
                   (char *)&on, sizeof(on)) < 0) {
        perror("setsockopt");
        close(fd);
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((unsigned short)port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    if (listen(fd, 10) < 0) {
        perror("listen");
        close(fd);
        return -1;
    }

    return fd;
}


static int
recv_request(int fd, char *buf, int len)
{
    int n;

    n = recv(fd, buf, len - 1, 0);
    if (n < 0) {
        perror("recv");
        return -1;
    }
    if (n == 0)
        return 0;
    buf[n] = '\0';
    return n;
}

static int
send_all(int fd, char *buf, int len)
{
    int sent;
    int n;

    sent = 0;
    while (sent < len) {
        n = send(fd, buf + sent, len - sent, 0);
        if (n < 0) {
            if (errno == EINTR)
                continue;
            return -1;
        }
        if (n == 0)
            return -1;
        sent += n;
    }
    return 0;
}


static void
send_response(int fd, int code, char *reason,
	      char *content_type, char *body)
{
    char header[REQ_SIZE];
    int body_len;
    int header_len;

    body_len = strlen(body);
    header_len = snprintf(header, sizeof(header),
                          "HTTP/1.1 %d %s\r\n"
                          "Server: simple_httpd\r\n"
                          "Content-Type: %s\r\n"
                          "Content-Length: %d\r\n"
                          "Connection: close\r\n"
                          "\r\n",
                          code, reason, content_type, body_len);
    send_all(fd, header, header_len);
    send_all(fd, body, body_len);
}

static char *
read_page(char *path)
{
    FILE	*fp;
    ssize_t	sz, rs;
    char	*cp;

    fp = fopen(path, "r");
    if (!fp) {
	return NULL;
    }
    fseek(fp, 0, SEEK_END);
    sz = ftell(fp);
    cp = malloc(sz);
    if (!cp) {
	printf("Cannot allocate memory. size = %ld\n", sz);
	exit(-1);
    }
    fseek(fp, 0, SEEK_SET);
    rs = fread(cp, sz, 1, fp);
    if (rs < 1) {
	printf("Cannot read all data.\n");
    }
    fclose(fp);
    return cp;
}

static int
handle_client(int client_fd)
{
    char	req[REQ_SIZE];
    char	method[BUF_SIZE];
    char	path[BUF_SIZE];
    char	version[BUF_SIZE];
    char	*cp;
    int		n;
    int		fields;

    n = recv_request(client_fd, req, sizeof(req));
    if (n <= 0)
        return -1;

    printf("RECEIVE:\n%s\n", req);
    fields = sscanf(req, "%255s %255s %255s", method, path, version);
    if (fields != 3) {
        send_response(client_fd, 400, "Bad Request", "text/plain",
                      "Bad Request\n");
        return -1;
    }

    if (strcmp(method, "GET") != 0) {
        send_response(client_fd, 405, "Method Not Allowed", "text/plain",
                      "Method Not Allowed\n");
        return -1;
    }

    if (path[0] == '/') {
	char	*src, *dst;
	src = &path[1]; dst = path;
	while ((*dst++ = *src++));
    }
    if (!path[0]) {
	strcpy(path, "index.html");
    }
    cp = read_page(path);
    if (cp) {
	send_response(client_fd, 200, "OK", "text/html", cp);
	free(cp);
    } else {
        send_response(client_fd, 404, "Not Found", "text/plain", "Not Found\n");
    }
    return 0;
}

static int
forever(int listen_fd)
{
    int		client_fd;
    struct sockaddr_in peer;
    socklen_t	peer_len;

    for (;;) {
        peer_len = sizeof(peer);
        client_fd = accept(listen_fd, (struct sockaddr *)&peer, &peer_len);
        if (client_fd < 0) {
            if (errno == EINTR)
                continue;
            perror("accept");
            return -1;
        }
        handle_client(client_fd);
        close(client_fd);
    }
}

int
main(int argc, char **argv)
{
    int port;
    int listen_fd;

    if (argc > 2) {
        usage(argv[0]);
        return 1;
    }
    if (argc == 2) {
        port = atoi(argv[1]);
    } else {
        port = DEFAULT_PORT;
    }
    if (port <= 0) {
        usage(argv[0]);
        return 1;
    }
    listen_fd = open_listen_socket(port);
    if (listen_fd < 0)
        return 1;

    printf("listening on http://127.0.0.1:%d/\n", port);
    forever(listen_fd);

    close(listen_fd);
    return 0;
}

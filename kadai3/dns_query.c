/*
 * dns_query.c
 *
 * Compile:
 *	cc -Wall -O2 -o dns_query dns_query.c
 * Usage:
 *   ./dns_query www.otsuma.ac.jp 8.8.8.8
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>

#define DNS_PORT        53
#define DNS_MAX_PACKET  512
#define DNS_TYPE_A      1
#define DNS_TYPE_CNAME  5
#define DNS_CLASS_IN    1

static void
put16(unsigned char *p, unsigned int v)
{
    p[0] = (v >> 8) & 0xff;
    p[1] = v & 0xff;
}

static unsigned int
get16(const unsigned char *p)
{
    return ((unsigned int)p[0] << 8) | p[1];
}

static unsigned long
get32(const unsigned char *p)
{
    return ((unsigned long)p[0] << 24) |
           ((unsigned long)p[1] << 16) |
           ((unsigned long)p[2] << 8) |
           p[3];
}

static int
encode_qname(unsigned char *buf, int buflen, const char *name)
{
    const char *p, *dot;
    int len, off;

    p = name;
    off = 0;
    while (*p != '\0') {
        dot = strchr(p, '.');
        if (dot != NULL)
            len = dot - p;
        else
            len = strlen(p);
        if (len <= 0 || len > 63)
            return -1;
        if (off + len + 1 >= buflen)
            return -1;
        buf[off++] = len;
        memcpy(buf + off, p, len);
        off += len;
        if (dot == NULL)
            break;
        p = dot + 1;
    }
    if (off >= buflen)
        return -1;
    buf[off++] = 0;
    return off;
}

static int
read_name(const unsigned char *msg, int msglen, int off,
          char *out, int outlen, int *next)
{
    int pos, len, outpos, jumped, loop;
    unsigned int ptr;

    pos = off;
    outpos = 0;
    jumped = 0;
    loop = 0;

    while (pos < msglen) {
        len = msg[pos];
        if ((len & 0xc0) == 0xc0) {
            if (pos + 1 >= msglen)
                return -1;
            ptr = ((len & 0x3f) << 8) | msg[pos + 1];
            if (!jumped)
                *next = pos + 2;
            pos = ptr;
            jumped = 1;
            if (++loop > msglen)
                return -1;

            continue;
        }
        if ((len & 0xc0) != 0)
            return -1;
        if (len == 0) {
            if (!jumped)
                *next = pos + 1;
            if (outpos == 0) {
                if (outlen < 2)
                    return -1;
                out[0] = '.';
                out[1] = '\0';
            } else {
                out[outpos - 1] = '\0';
            }
            return 0;
        }
        pos++;
        if (pos + len > msglen)
            return -1;
        if (outpos + len + 1 >= outlen)
            return -1;
        memcpy(out + outpos, msg + pos, len);
        outpos += len;
        out[outpos++] = '.';
        pos += len;
    }
    return -1;
}

int
main(int argc, char **argv)
{
    unsigned char query[DNS_MAX_PACKET];
    unsigned char answer[DNS_MAX_PACKET];
    unsigned short id;
    const char *hostname;
    const char *dns_server;
    struct sockaddr_in addr;
    struct timeval tv;
    int sock;
    int qname_len;
    int query_len;
    int n;
    int off;
    int i;
    unsigned int flags;
    unsigned int qdcount;
    unsigned int ancount;
    unsigned int rcode;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s hostname [dns-server]\n", argv[0]);
        return 1;
    }

    hostname = argv[1];
    dns_server = (argc >= 3) ? argv[2] : "8.8.8.8";

    memset(query, 0, sizeof(query));

    srand((unsigned int)time(NULL));
    id = rand() & 0xffff;

    put16(query + 0, id);        /* ID */
    put16(query + 2, 0x0100);    /* recursion desired */
    put16(query + 4, 1);         /* QDCOUNT */
    put16(query + 6, 0);         /* ANCOUNT */
    put16(query + 8, 0);         /* NSCOUNT */
    put16(query + 10, 0);        /* ARCOUNT */

    qname_len = encode_qname(query + 12, DNS_MAX_PACKET - 12, hostname);
    if (qname_len < 0) {
        fprintf(stderr, "invalid hostname: %s\n", hostname);
        return 1;
    }

    off = 12 + qname_len;
    put16(query + off, DNS_TYPE_A);
    off += 2;
    put16(query + off, DNS_CLASS_IN);
    off += 2;
    query_len = off;

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        return 1;
    }
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                   &tv, sizeof(tv)) < 0) {
        perror("setsockopt");
        close(sock);
        return 1;
    }
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(DNS_PORT);
    if (inet_pton(AF_INET, dns_server, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid DNS server address: %s\n", dns_server);
        close(sock);
        return 1;
    }
    if (sendto(sock, query, query_len, 0,
               (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("sendto");
        close(sock);
        return 1;
    }
    n = recvfrom(sock, answer, sizeof(answer), 0, NULL, NULL);
    if (n < 0) {
        perror("recvfrom");
        close(sock);
        return 1;
    }

    close(sock);

    if (n < 12) {
        fprintf(stderr, "short DNS response\n");
        return 1;
    }
    if (get16(answer + 0) != id) {
        fprintf(stderr, "DNS transaction ID mismatch\n");
        return 1;
    }

    flags = get16(answer + 2);
    qdcount = get16(answer + 4);
    ancount = get16(answer + 6);
    rcode = flags & 0x000f;

    if (rcode != 0) {
        fprintf(stderr, "DNS error: rcode=%u\n", rcode);
        return 1;
    }

    off = 12;
    for (i = 0; i < (int)qdcount; i++) {
        char name[256];
        if (read_name(answer, n, off, name, sizeof(name), &off) < 0) {
            fprintf(stderr, "failed to parse question name\n");
            return 1;
        }
        if (off + 4 > n) {
            fprintf(stderr, "broken question section\n");
            return 1;
        }
        off += 4;
    }

    printf("DNS server: %s\n", dns_server);
    printf("query: %s A\n\n", hostname);

    for (i = 0; i < (int)ancount; i++) {
        char name[256];
        unsigned int type;
        unsigned int class_;
        unsigned long ttl;
        unsigned int rdlen;

        if (read_name(answer, n, off, name, sizeof(name), &off) < 0) {
            fprintf(stderr, "failed to parse answer name\n");
            return 1;
        }
        if (off + 10 > n) {
            fprintf(stderr, "broken answer section\n");
            return 1;
        }
        type = get16(answer + off);
        off += 2;
        class_ = get16(answer + off);
        off += 2;
        ttl = get32(answer + off);
        off += 4;
        rdlen = get16(answer + off);
        off += 2;
        if (off + rdlen > n) {
            fprintf(stderr, "broken rdata\n");
            return 1;
        }

        if (class_ == DNS_CLASS_IN && type == DNS_TYPE_A && rdlen == 4) {
            char ip[INET_ADDRSTRLEN];

            if (inet_ntop(AF_INET, answer + off, ip, sizeof(ip)) != NULL)
                printf("%s\t%lu\tA\t%s\n", name, ttl, ip);
        } else if (class_ == DNS_CLASS_IN && type == DNS_TYPE_CNAME) {
            char cname[256];
            int dummy;

            if (read_name(answer, n, off, cname, sizeof(cname), &dummy) == 0)
                printf("%s\t%lu\tCNAME\t%s\n", name, ttl, cname);
        }
        off += rdlen;
    }
    return 0;
}

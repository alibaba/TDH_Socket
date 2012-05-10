/*
* Copyright(C) 2011-2012 Alibaba Group Holding Limited
* 
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*/


#include <easy_inet.h>
#include <easy_string.h>
#include <easy_atomic.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <netdb.h>
#include <arpa/inet.h>      // inet_addr
#include <sys/ioctl.h>
#include <linux/if.h>

/**
 * 把sockaddr_in转成string
 */
char *easy_inet_addr_to_str(easy_addr_t *ptr, char *buffer, int len)
{
    struct sockaddr_in      *addr;
    unsigned char           *b;

    addr = (struct sockaddr_in *) ptr;
    b = (unsigned char *) &addr->sin_addr.s_addr;

    if (addr->sin_port)
        snprintf(buffer, len, "%d.%d.%d.%d:%d", b[0], b[1], b[2], b[3], ntohs(addr->sin_port));
    else
        snprintf(buffer, len, "%d.%d.%d.%d", b[0], b[1], b[2], b[3]);

    return buffer;
}

/**
 * 把str转成addr(用uint64_t表示,IPV4)
 */
easy_addr_t easy_inet_str_to_addr(const char *host, int port)
{
    easy_addr_t             address = {0, 0};
    char                    *p, buffer[64];
    int                     len;

    if (host && (p = strchr(host, ':')) != NULL) {
        if ((len = p - host) > 63)
            return address;

        memcpy(buffer, host, len);
        buffer[len] = '\0';
        host = buffer;

        if (!port)
            port = atoi(p + 1);
    }

    // parse host
    easy_inet_parse_host(&address, host, port);

    return address;
}

/**
 * 是IP地址, 如: 192.168.1.2
 */
int easy_inet_is_ipaddr(const char *host)
{
    unsigned char        c, *p;

    p = (unsigned char *)host;

    while ((c = (*p++)) != '\0') {
        if ((c != '.') && (c < '0' || c > '9')) {
            return 0;
        }
    }

    return 1;
}

/**
 * 解析host
 */
int easy_inet_parse_host(easy_addr_t *address, const char *host, int port)
{
    struct sockaddr_in  addr;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);

    if (host && host[0]) {
        // 如果是ip地址,  用inet_addr转一下, 否则用gethostbyname
        if (easy_inet_is_ipaddr(host)) {
            if ((addr.sin_addr.s_addr = inet_addr(host)) == INADDR_NONE)
                return EASY_ERROR;
        } else {
            // FIXME: gethostbyname会阻塞
            char    buffer[1024];
            struct  hostent h, *hp;
            int     rc;

            if (gethostbyname_r(host, &h, buffer, 1024, &hp, &rc) || hp == NULL)
                return EASY_ERROR;

            addr.sin_addr.s_addr = *((in_addr_t *) (hp->h_addr));
        }
    } else {
        addr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    memcpy(address, &addr, sizeof(easy_addr_t));

    return EASY_OK;
}

/**
 * 得到本机所有IP
 */
int easy_inet_hostaddr(uint64_t *address, int size)
{
    int             fd, ret, n;
    struct ifconf   ifc;
    struct ifreq    *ifr;

    ret = 0;

    if ((fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
        return 0;

    ifc.ifc_len = sizeof(struct ifreq) * size;
    ifc.ifc_buf = (char *) malloc(ifc.ifc_len);

    if (ioctl(fd, SIOCGIFCONF, (char *)&ifc) < 0)
        goto out;

    ifr = ifc.ifc_req;

    for (n = 0; n < ifc.ifc_len; n += sizeof(struct ifreq)) {
        memcpy(&address[ret++], &(ifr->ifr_addr), sizeof(uint64_t));
        ifr++;
    }

out:
    free(ifc.ifc_buf);
    close(fd);
    return ret;
}

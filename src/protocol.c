
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "protocol.h"
#include "wrappers.h"
#include "debug.h"


int proto_send_packet(int fd, XACTO_PACKET *pkt, void *data){
    int olderrno = errno;
    debug("pkt: %p", pkt);
    debug("data: %p", data);
    debug("sending packet: (timestamp=%d.%d, size=%d, serial=%d)", pkt->timestamp_sec, pkt->timestamp_nsec, pkt->size, pkt->serial);

    Rio_writen(fd, pkt, sizeof(XACTO_PACKET));
    if(pkt->size && data){
        int payload_size = ntohl(pkt->size);
        Rio_writen(fd, data, payload_size);
    }
    return errno == olderrno ? 0 : -1;
}

int proto_recv_packet(int fd, XACTO_PACKET *pkt, void **datap){
    
    /* for error checking */
    int olderrno = errno;

    /* clear garbage pointer */
    memset(pkt, 0, sizeof(XACTO_PACKET));
    
    /* read header data */
    debug("reading header...");
    ssize_t nread = Read(fd, pkt, sizeof(XACTO_PACKET));
    if(nread <= 0){
        if(nread == 0) return -1;
        app_error("proto_recv");
    }
    else if(nread > 0 && nread < sizeof(XACTO_PACKET)){
        errno = EIO;
        app_error("short write");
    }
    else {
        int payload_size = ntohl(pkt->size);

        if(payload_size > 0x100000){
            error("size error: %i", payload_size);
            errno = E2BIG;
            return -1;
        }

        void *buf = malloc(sizeof(pkt->size));
        void *bufptr = buf;
        
        while(1){
            if(payload_size == 0){
                if(datap == NULL) {
                    free(buf);
                } else {
                    *datap = buf;
                }
                break;
            }
            debug("reading payload data...");
            nread = Read(fd, bufptr, payload_size);
            if(nread == -1){
                free(buf);
                break;
            }
            payload_size -= nread;
            bufptr += payload_size;
        }
    }

    return errno == olderrno ? 0 : -1;
}
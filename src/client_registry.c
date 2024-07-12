#include <fcntl.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "client_registry.h"
#include "settings.h"
#include "wrappers.h"
#include "debug.h"

struct client_registry
{
    int *buf;       /* Buffer array */
    int n;          /* Maximum number of slots */
    int front;      /* buf[(front+1)%n] is first item */
    int back;       /* buf[back%n] is last item */
    sem_t mutex;    /* Protects accesses to buf */
    sem_t slots;    /* Counts available slots */
    sem_t items;    /* Counts available items */
    pthread_mutex_t lock; /* lock to wait for items to be zero */
    pthread_mutex_t cond; /* declaring mutex */
};

void swap(int *a, int *b){
    int tmp = *a;
    *a = *b;
    *b = tmp;
}

long get_self(){
    return (long)Pthread_self()->__sig
}


CLIENT_REGISTRY *creg_init(){
    info("%ld: Initialize client registry", Pthread_self()->__sig);
    CLIENT_REGISTRY *cr = malloc(sizeof(CLIENT_REGISTRY));
    cr->buf = calloc(MAX_CLIENTS, sizeof(int));
    cr->n = MAX_CLIENTS;
    memset(cr->buf, -1, sizeof(int)*MAX_CLIENTS);
    cr->front = cr->back = -1;
    Sem_init(&cr->mutex, 0, 1); // binary sem for locking
    Sem_init(&cr->slots, 0, MAX_CLIENTS); // initially buf has n empty slots
    Sem_init(&cr->items, 0, 0); //initially buf has zero items
    return cr;
}

void creg_fini(CLIENT_REGISTRY *cr){
    if(cr == NULL) return; // sanity check;
    free(cr->buf);
    free(cr);
}

bool fd_is_valid(int fd)
{
    return (fcntl(fd, F_GETFD) != -1 || errno != EBADF);
}

int creg_register(CLIENT_REGISTRY *cr, int fd){
    
    if(cr == NULL) return -1; // sanity check;
    if(!fd_is_valid(fd)) return -1; // sanity check
    P(&cr->slots);                          /* Wait for available slot */
    P(&cr->mutex);                          /* Lock the buffer */
    cr->buf[(++cr->back)%(cr->n)] = fd;     /* Insert the item */
    V(&cr->mutex);                          /* Unlock the buffer */
    V(&cr->items);                          /* Announce available item */ 
    int val; sem_getvalue(&cr->items, &val);
    info("%ld: Register client fd %i (total connected: %i)", get_self(),fd, val);
    return 0;
}

int creg_unregister(CLIENT_REGISTRY *cr, int fd){
    int item;
    P(&cr->items);                                  /* Wait for available */ 
    P(&cr->mutex);                                  /* Lock the buffer */
    int found = 0;
    for(int i = 0; i < cr->n; i++){      /* Looking for the fd we need and moving it to the front of the queue */
        if(!found && cr->buf[i] == fd) {
            found = 1;
            int pop = (cr->front+1)%(cr->n);
            // swap
            int temp = cr->buf[pop];
            cr->buf[pop] = cr->buf[i];
            cr->buf[i] = temp;
            // swap(&cr->buf[pop], &cr->buf[i]);
        }
    }
    if(found == 0){
        error("%ld: client fd %i is not registered", get_self(), fd);
        V(&cr->mutex); /* unlock buffer */
        V(&cr->items); /* announce items are unchanged */
        return -1;
    }
    item = cr->buf[(++cr->front)%(cr->n)];          /* Remove the item */
    int val; sem_getvalue(&cr->items, &val);
    info("%ld: Unregister client fd %i (total connected: %i)", get_self(), item, val);
    V(&cr->mutex);                                  /* Unlock the buffer */
    V(&cr->slots);                                  /* Announce available slot */
    return 0;
}
void creg_wait_for_empty(CLIENT_REGISTRY *cr){
    int val;
    while(1){
        sem_getvalue(&cr->items, &val);
        if(val == 0) return;
    }
}
void creg_shutdown_all(CLIENT_REGISTRY *cr){
    for(int i = 0; i < cr->n; ++i){
        if(cr->buf[i] > 0){
            if(shutdown(cr->buf[i], SHUT_RDWR) < 0){
                unix_error("error shutting down fd");
            }
        }
    }
    return;
}
#include <getopt.h>
#include <errno.h>

#include "settings.h"
#include "debug.h"
#include "client_registry.h"
#include "transaction.h"
#include "tpool.h"
#include "store.h"
#include "server.h"
#include "wrappers.h"

static void terminate(int status);

CLIENT_REGISTRY *client_registry;
tpool_t *pool;

void hangup_handler(int sig);

void *thread(void *vargp) {
    Pthread_detach(Pthread_self());
    void *fp = xacto_client_service(vargp);
    free(fp);
    return NULL; 
}

int main(int argc, char* argv[]){
    // Option processing should be performed here.
    // Option '-p <port>' is required in order to specify the port number
    // on which the server should listen.
    int c;
    char *port;
    opterr = 0;
    #define PORT_OPTION 'p'
    while((c = getopt(argc, argv, "p:")) != -1){
        switch (c)
        {
        case PORT_OPTION:
            //TOOD ERROR CHECK
            port = optarg;
            break;
        case '?':
            if(optopt == PORT_OPTION){
                error("-%c requires an argument.", PORT_OPTION);
                terminate(EXIT_FAILURE);
            } else {
                error("unknown option -%c.", optopt);
                terminate(EXIT_FAILURE);
            }
        default:
            terminate(EXIT_FAILURE);
        }
    }
    if(optind < argc) terminate(EXIT_FAILURE);
    info("Option validation complete");
    // Perform required initializations of the client_registry,
    // transaction manager, and object store.
    client_registry = creg_init();
    trans_init();
    store_init();

    // pool = tpool_init(10); // set up thread pool
    // TODO: Set up the server socket and enter a loop to accept connections
    // on this socket.  For each connection, a thread should be started to
    // run function xacto_client_service().  In addition, you should install
    // a SIGHUP handler, so that receipt of SIGHUP will perform a clean
    // shutdown of the server.
    
    /* install sighup handhler */
    Signal(SIGHUP, hangup_handler);
    
    /* setting up server socket */
    int listenfd, *connfd;
    char client_name[MAXLINE], client_port[MAXLINE];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;
    pthread_t tid;

    listenfd = Open_listenfd(port);
    info("Listening on port %s ...", port);
    while(1){
        clientlen = sizeof(clientaddr);
        connfd = malloc(sizeof(int));
        *connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_name, MAXLINE, client_port, MAXLINE, 0);
        info("Accepted connection from (%s, %s)\n", client_name, client_port);

        Pthread_create(&tid, NULL, thread, connfd);
    }
    terminate(EXIT_FAILURE);
}

/*
 * Function called to cleanly shut down the server.
 */
void terminate(int status) {
    // Shutdown all client connections.
    // This will trigger the eventual termination of service threads.
    creg_shutdown_all(client_registry);
    
    debug("Waiting for service threads to terminate...");
    creg_wait_for_empty(client_registry);
    debug("All service threads terminated.");

    // Finalize modules.
    creg_fini(client_registry);
    trans_fini();
    store_fini();

    // tpool_destroy(pool);

    debug("Xacto server terminating");
    exit(status);
}

void hangup_handler(int sig){
    int status = (int)errno;
    terminate(status);
}
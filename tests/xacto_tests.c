#include <criterion/criterion.h>
#include <pthread.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <string.h>
#include <wait.h>

#include <sys/socket.h>

#include "client_registry.h"
#include "data.h"

static void init() {
#ifndef NO_SERVER
    int ret;
    int i = 0;
    do { // Wait for server to start
	ret = system("netstat -an | fgrep '0.0.0.0:9999' > /dev/null");
	sleep(1);
    } while(++i < 30 && WEXITSTATUS(ret));
#endif
}

static void startup() {
    int pid = 0;
    if((pid = fork()) == 0){
        system("bin/xacto -p 9999");
    }
}

static void teardown() {
    system("kill $(lsof -t -i:9999)");
}

static void fini() {
}

/*
 * Thread to run a command using system() and collect the exit status.
 */
void *system_thread(void *arg) {
    long ret = system((char *)arg);
    return (void *)ret;
}

// Criterion seems to sort tests by name.  This one can't be delayed
// or others will time out.
Test(student_suite, 00_start_server, .timeout = 30) {
    fprintf(stderr, "server_suite/00_start_server\n");
    int server_pid = 0;
    int ret = system("netstat -an | fgrep '0.0.0.0:9999' > /dev/null");
    cr_assert_neq(WEXITSTATUS(ret), 0, "Server was already running");
    fprintf(stderr, "Starting server...");
    if((server_pid = fork()) == 0) {
    /* 
    int fd = open("rsrc/server_output", O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);
     */
	execlp("valgrind", "xacto", "--leak-check=full", "--show-leak-kinds=all", "--track-fds=yes",
	       "--error-exitcode=37", "--log-file=valgrind.out", "bin/xacto", "-p", "9999", NULL);
	fprintf(stderr, "Failed to exec server\n");
	abort();
    }
    fprintf(stderr, "pid = %d\n", server_pid);
    char *cmd = "sleep 10";
    pthread_t tid;
    pthread_create(&tid, NULL, system_thread, cmd);
    pthread_join(tid, NULL);
    cr_assert_neq(server_pid, 0, "Server was not started by this test");
    fprintf(stderr, "Sending SIGHUP to server pid %d\n", server_pid);
    kill(server_pid, SIGHUP);
    sleep(5);
    kill(server_pid, SIGKILL);
    wait(&ret);
    fprintf(stderr, "Server wait() returned = 0x%x\n", ret);
    if(WIFSIGNALED(ret)) {
	fprintf(stderr, "Server terminated with signal %d\n", WTERMSIG(ret));	
	system("cat valgrind.out");
	if(WTERMSIG(ret) == 9)
	    cr_assert_fail("Server did not terminate after SIGHUP");
    }
    if(WEXITSTATUS(ret) == 37)
	system("cat valgrind.out");
    cr_assert_neq(WEXITSTATUS(ret), 37, "Valgrind reported errors");
    cr_assert_eq(WEXITSTATUS(ret), 0, "Server exit status was not 0");
}

Test(student_suite, 01_connect, .init = init, .fini = fini, .timeout = 5) {
    fprintf(stderr, "server_suite/01_connect\n");
    int ret = system("util/client -p 9999 </dev/null | grep 'Connected to server'");
    cr_assert_eq(ret, 0, "expected %d, was %d\n", 0, ret);
}

Test(student_suite, 02_put, .init = init, .fini = fini, .timeout = 5) {
    fprintf(stderr, "server_suite/02_put\n");
    FILE *fp;
    char line[BUFSIZ];
    char *lineptr = line;
    size_t n = BUFSIZ;
    char *cmd = "util/client -p 9999 -q < rsrc/02_put";
    fp = popen(cmd, "r");
    cr_assert_not_null(fp, "problem opening file");
    int expected_payload = 1;
    while( getline(&lineptr, &n, fp) > 0 ){
        #define NUM_PARAMS 3
        char type[6];
        int null;
        int payload;
        if(sscanf(line, "%*f: type=%s, serial=%*i, status=%*i, size=%*i, null=%i, payload=[%i]", type, &null, &payload) == NUM_PARAMS){
            cr_expect_eq(null, 0, "payload is null");
            cr_expect_eq(payload, expected_payload, "Expected %i, was %i", payload, expected_payload);
            expected_payload++;
        }
    }
}

CLIENT_REGISTRY *test_registry;

void *thread(void *vargp){
    pthread_detach(pthread_self());
    fprintf(stderr, "starting thread... \n");
    int *myfd = (int *)vargp;
    unsigned int seed = (unsigned int)(time(NULL) ^ getpid() ^ pthread_self());
    creg_register(test_registry, *myfd);
    sleep(rand_r(&seed) % 5); // sleep from 0-5 seconds
    close(*myfd);
    creg_unregister(test_registry, *myfd);
    return NULL;
}

Test(student_suite, 03_creg, .timeout = 10, .disabled = true) {
    fprintf(stderr, "server_suite/03_creg\n");

    fprintf(stderr, "initializing client registry...\n");
    test_registry = creg_init();
    fprintf(stderr, "saving initial bytes... \n");
    char *init_bytes = malloc(sizeof(test_registry));
    memmove(init_bytes, (char*)test_registry, sizeof(test_registry));

    fprintf(stderr, "creating threads...\n");
    #define NUM_CLIENTS 500
    int fds[NUM_CLIENTS];
    pthread_t tid;
    for(int i = 0; i < NUM_CLIENTS; ++i){
        fds[i] = socket(AF_LOCAL, SOCK_RAW, 0);
        pthread_create(&tid, NULL, thread, (void *)&fds[i]);
    }
    pthread_join(tid, NULL);
    fprintf(stderr, "waiting... \n");
    creg_wait_for_empty(test_registry);
    fprintf(stderr, "check all fds are gone... \n"); 
    int n = sizeof(test_registry);
    for(char *p = (char*)test_registry, *q = init_bytes; n--; ++p, ++q){
        cr_expect_eq(*p, *q, "[Registry at addr %p]: Expected %c at address %p, was %c", test_registry, *q, p, *p);
    }
    free(init_bytes);
    creg_fini(test_registry);
    #undef NUM_CLIENTS
}

Test(student_suite, 04_creg, .timeout = 5, .disabled = true){
    CLIENT_REGISTRY *cr = creg_init();
    int fd = socket(AF_LOCAL, SOCK_RAW, 0);
    cr_assert_eq(0, creg_register(cr, fd));
    cr_assert_eq(-1, creg_unregister(cr, 0));
    
    creg_shutdown_all(cr); printf("shutdown\n");
    int val = send(fd, "1", 1, 0);
    cr_assert_eq(val, -1);
    cr_assert_eq(0, creg_unregister(cr, fd));
    
    creg_fini(cr);
}

// Test(student_suite, 05_blob, .timeout = 5){
//     BLOB *blob = blob_create("sadlfkm", 8);
//     blob_ref(blob, "");
//     blob_ref(blob, "");
// }
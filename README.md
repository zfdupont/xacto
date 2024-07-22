## The Xanadu Server and Protocol: Overview

The "Xanadu" server implements a simple transactional object store,
designed for use in a network environment.  For the purposes of this
assignment, an object store is essentially a hash map that maps
**keys** to **values**, where both keys and values can be arbitrary data.
There are two basic operations on the store: `PUT` and `GET`.
As you might expect, the `PUT` operation associates a key with a value,
and the `GET` operation retrieves the value associated with a key.
Although values may be null, keys may not.  The null value is used as
the default value associated with a key if no `PUT` has been performed
on that key.

A client wishing to make use of the object store first makes a network
connection to the server.  Upon connection, a **transaction** is
created by the server.  All operations that the client performs during
this session will execute in the scope of that transaction.
The client issues a series of `PUT` and `GET` requests to the server,
receiving a response from each one.  Upon completing the series of
requests, the client issues a `COMMIT` request.  If all is well, the
transaction **commits** and its effects on the store are made permanent.
Due to interference of transactions being executed concurrently by
other clients, however, it is possible for a transaction to **abort**
rather than committing.  The effects of an aborted transaction are
erased from the store and it is as if the transaction had never happened.
A transaction may abort at any time up until it has successfully committed;
in particular the response from any `PUT` or `GET` request might
indicate that the transaction has aborted.  When a transaction has
aborted, the server closes the client connection.  The client may
then open a new connection and retry the operations in the context of
a new transaction.

The Xanadu server architecture is that of a multi-threaded network
server.  When the server is started, a **master** thread sets up a
socket on which to listen for connections from clients.  When a
connection is accepted, a **client service thread** is started to
handle requests sent by the client over that connection.  The client
service thread executes a service loop in which it repeatedly receives
a **request packet** sent by the client, performs the request, and
sends a **reply** packet that indicates the result.
Besides request and reply packets, there are also **data** packets
that are used to send key and value data between the client and
server.

> :nerd: One of the basic tenets of network programming is that a
> network connection can be broken at any time and the parties using
> such a connection must be able to handle this situation.  In the
> present context, the client's connection to the Xanadu server may
> be broken at any time, either as a result of explicit action by the
> client or for other reasons.  When disconnection of the client is
> noticed by the client service thread, the client's transaction
> is aborted and the client service thread terminates.

### The Base Code

The base code consists of header files that define module interfaces,
a library `xanadu.a` containing binary object code for our
implementations of the modules, and a source code file `main.c` that
contains containing a stub for function `main()`.  The `Makefile` is
designed to compile any existing source code files and then link them
against the provided library.  The result is that any modules for
which you provide source code will be included in the final
executable, but modules for which no source code is provided will be
pulled in from the library.

The `util` directory contains an executable for a simple test client.
When you run this program it will print a help message that
summarizes the commands.  The client program understands command-line
options `-h <hostname>`, `-p <port>`, and `-q`.  The `-p` option is
required; the others are options.  The `-q` flag tells the client to
run without prompting; this is useful if you want to run the client
with the standard input redirected from a file.

## Task I: Server Initialization

When the base code is compiled and run, it will print out a message
saying that the server will not function until `main()` is
implemented.  The `main()` function will do the following things:

- Obtain the port number to be used by the server from the command-line
  arguments.  The port number is to be supplied by the required option
  `-p <port>`.

- Install a `SIGHUP` handler so that clean termination of the server can
  be achieved by sending it a `SIGHUP`.  Note that you need to use
  `sigaction()` rather than `signal()`, as the behavior of the latter is
  not well-defined in a multithreaded context.

- Set up the server socket and enter a loop to accept connections
  on this socket.  For each connection, a thread should be started to
  run function `xanadu_client_service()`.

## Task II: Send and Receive Functions

The header file `include/protocol.h` defines the format of the packets
used in the Xanadu network protocol.  The concept of a protocol is an
important one to understand.  A protocol creates a standard for
communication so that any program implementing a protocol will be able
to connect and operate with any other program implementing the same
protocol.  Any client should work with any server if they both
implement the same protocol correctly.  In the Xanadu protocol,
clients and servers exchange **packets** with each other.  Each packet
has two parts: a fixed-size part that describes the packet, and an
optional **payload** that can carry arbitrary data.  The fixed-size
part of a packet always has the same size and format, which is given
by the `xanadu_packet` structure; however the payload can be of arbitrary
size.  One of the fields in the fixed-size part tells how long the payload is.

- The function `proto_send_packet` is used to send a packet over a
network connection.  The `fd` argument is the file descriptor of a
socket over which the packet is to be sent.  The `pkt` argument is a
pointer to the fixed-size part of the packet.  The `data` argument is a
pointer to the data payload, if there is one, otherwise it is `NULL`.
The `proto_send_packet` assumes that multi-byte fields in the packet
passed to it are in **host byte order**, which is the normal way that
values are stored in variables.  However, as byte ordering
(i.e. "endianness") differs between computers, before sending the
packet it is necessary to convert any multi-byte fields to **network
byte order**.  This can be done using, e.g., the `htonl()` and related
functions described in the Linux man pages.  Once the header has been
converted to network byte order, the `write()` system call is used to
write the header to the "wire" (i.e. the network connection).  If the
length field of the header specifies a nonzero payload length, then an
additional `write()` call is used to write the payload data to the
wire.

- The function `proto_recv_packet()` reverses the procedure in order to
receive a packet.  It first uses the `read()` system call to read a
fixed-size packet header from the wire.  After converting multi-byte fields
in the header from network byte order to host byte order (see the man
page for `ntohl()`), if the length field of the header is nonzero then
an additional `read()` is used to read the payload from the wire.  The
header and payload are stored using pointers supplied by the caller.

## Task III: Client Registry

You probably noticed the initialization of the `client_registry`
variable in `main()` and the use of the `creg_wait_for_empty()`
function in `terminate()`.  The client registry provides a way of
keeping track of the number of client connections that currently exist,
and to allow a "master" thread to forcibly shut down all of the
connections and to await the termination of all server threads
before finally terminating itself.

The functions provided by a client registry are specified in the
`client_registry.h` header file. These functions
need to be thread-safe, so a mutex is used to protect access to 
the thread counter data.  A semaphore is used
to perform the required blocking in the `creg_wait_for_empty()`
function.  To shut down a client connection, the `shutdown()`
function described in Section 2 of the Linux manual pages is used.

## Task IV: Data Module

The file `data.h` describes **blobs**, which represent reference-counted
chunks of arbitrary data, **keys**, which are blobs packaged together
with a hash code so that they can be used as keys in a hash table,
and **versions**, which are used to represent versions of data in the
transactional store.

A possibly unfamiliar (but essential) concept here is the notion of a
**reference count**.
The basic idea of reference counting is for an object to maintain
a field that counts the total number of pointers to it that exist.
Each time a pointer is created (or copied) the reference count is
increased.  Each time a pointer is discarded, the reference count
is decreased.  When the reference count reaches zero, there are
no outstanding pointers to the object and it can be freed.
The reason we need to use reference counting for various objects
is because pointers to those objects can be stored in other objects,
where they can persist long after the original context in which
they were created has terminated.  Eventually, we need to be able to
determine when the last pointer to an object has been discarded,
so that we can free the object and not leak storage.

In a reference counting scheme, the object to which reference counting
is to be applied has to provide two functions: one for increasing
the reference count and another for decreasing it.  The function
for decreasing the reference count has the responsibility of
freeing the object when the reference count reaches zero.
In addition, the code that uses a reference-counted object has
to be carefully written so that whenever a pointer is created
the method to increase the reference count is called, and whenever
a pointer is discarded the method to decrease the reference count
is called.

Note that, in a multi-threaded setting, reference counts are shared
between threads and therefore need to be protected by mutexes if
they are to work reliably.


## Task V: Client Service Thread

Next, you should implement the thread function that performs service
for a client.  This function is called `xanadu_client_service`, and
you should implement it in the `src/server.c` file.

The `xanadu_client_service` function is invoked as the thread function
for a thread that is created to service a client connection.
The argument is a pointer to the integer file descriptor to be used
to communicate with the client.  Once this file descriptor has been
retrieved, the storage it occupied needs to be freed.
The thread must then become detached, so that it does not have to be
explicitly reaped, and it must register the client file descriptor with
the client registry.  Next, a transaction should be created to be used
as the context for carrying out client requests.
Finally, the thread should enter a service loop in which it repeatedly
receives a request packet sent by the client, carries out the request,
and sends a reply packet, followed (in the case of a reply to a `GET` request)
by a data packet that contains the result.  If as a result of carrying
out any of the requests, the transaction commits or aborts, then
(after sending the required reply to the current request) the service
loop should end and the client service thread terminate, closing the
client connection.

## Task VI: Transaction Manager

The transaction manager is responsible for creating, committing, and aborting transactions.
The various functions that have to be implemented are specified in
`transaction.h` header file.

For transactions, the function `trans_ref()` is used to increase
the reference count on a transaction and the function `trans_unref()`
is used to decrease the reference count.  The reference count itself is
maintained in the `refcnt` field of the `transaction` structure.
Because it is accessed concurrently, it needs to be protected by a mutex.
When transaction is created by the client service thread, it initially
has a reference count of one.
The transaction gets passed to various other functions, including operations
on the store.  If the store needs to save a reference to a transaction
(*e.g.* to record the transaction as the "creator" of a version of
some data), then it will increase the reference count before doing so
to account for the saved pointer.
Ultimately, the transaction will be committed via a call to
`trans_commit` or aborted via a call to `trans_abort`.
These two calls consume a reference to the transaction
(*i.e.* they decrement the reference count), so the caller should not
continue to use the transaction subsequently unless an additional
reference was previously created.

A transaction is only ever committed by a call to `trans_commit` made
by the client service thread in response to a `COMMIT` request received
from the client.  Once `trans_commit` has been called, it could be
that the last reference to the transaction has been consumed and
the transaction has been freed, so the transaction object should not
referenced again after that (unless another reference has previously
been obtained).
Calls to operations on the store may cause a transaction to abort,
but in that case they arrange to increase the reference count before doing
so, so that the net value of the reference count will not be decreased
and client service thread will be able to check the status of a
transaction after performing a store operation to see whether or not
the transaction has aborted.

One other complication in the implementation of transactions is
the notion of "dependency".  As a result of performing an operation on
the store, a transaction may become **dependent** on one or more other
transactions.  Before a transaction may commit, it must wait for all
the transactions on which it depends (*i.e.* the transactions in its
**dependency set**) to commit.  If any of the transactions in the dependency
set aborts, then the transaction itself must abort.
In order to manage dependencies, the transaction manager module provides
a function `trans_add_dependency`, which is used to add one transaction
(the "dependee") to the dependency set of another (the "dependent").
Adding a dependency is done by allocating a "dependency" structure,
storing a reference to the dependee transaction in that structure,
and adding the structure to the linked list of dependencies for the dependent
transaction, after checking to make sure that there is not already an entry
for the same dependee in the dependency list.  The reference count of the
dependee transaction must increased to account for the stored pointer.

When a transaction attempts to commit, it must first call `sem_wait`
on the semaphore in the `sem` field of each of the transactions in its
dependency set.  This will block the transaction until the transaction
on which it depends has committed and done `sem_post` on that semaphore.
Since a transaction can exist in the dependency set of more than one
other transaction, in order to know how many times to call `sem_post`
it has to rely on the transaction calling `sem_wait` to have first
incremented the `waitcnt` field on the transaction for which it is
waiting, so that `waitcnt` always correctly reflects the number of
transactions waiting on `sem_wait`.  Of course, the `waitcnt` field
of a transaction must only be accessed while the mutex lock on that
transaction is held.

Once `sem_wait` has been called on all transactions in the dependency
set, a transaction can check the status of these transactions to see
if any have aborted.  If so, then committing is not possible, and the
dependent transaction must abort instead.  If all of the dependee
transactions have committed, then the dependent transaction can move
to the "committed" state.

## Task VII: Transactional Store

The functions for the store are specified in the `store.h` header
file, where some further details of how the store works are given.
Besides the fairly straightforward initialization and finalization
functions, the store provides functions `store_put` and `store_get`,
which in the end are rather similar to each other.
Each function uses the hash code of the key passed as argument
to identify a particular "bucket" in the hash table.
The bucket is then searched to find a map entry whose key is equal
to the given key.  If an entry is not found, one is created and
inserted into the proper bucket.

Once the map entry for the given key has been located (or created),
then both `store_put` and `store_get` work by inserting a new
"version" into the list of versions contained in that map entry.
The concept of a version, and the rules by which version lists
are maintained, are detailed in `store.h`.
As a result of these rules, it is possible for a transaction calling
`store_put` or `store_get` to either abort or become dependent
on other transactions.  The implementations of these functions have
to arrange for these actions to be carried out when necessary.

Note that the only time versions are removed from the transactional
store is during the "garbage collection" phase carried out at the
beginning of a `store_put` or `store_get` operation.
Upon completion of an operation, there will in general be "extra"
versions in the store that will get cleaned up on the next call
to `store_put` or `store_get`.  When the server terminates
(as a result of `SIGHUP`), all objects will be cleanly freed.
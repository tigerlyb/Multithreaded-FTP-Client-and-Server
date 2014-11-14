Multithreaded FTP Client and Server in C
=============================================================

The aim of this project is to introduce to you the design issues involved in multi-threaded servers. In this project, both the client and server are multithreaded. The client will be able to handle multiple commands (from same user) simultaneously and server should be able to handle multiple commands from multiple clients. The overall interaction between the user and the system is similar except as indicated below. The client and server will support a set of commands as indicated in the following: get, put, delete, ls, cd, mkdir, pwd, quit. In addition, they will support one more command called “terminate”, which is used for terminating a long-running command (e.g., get and put on large files) from the same client.

The syntax of the terminate command is as follows:

terminate <command-ID> -- terminate the command identified by <command-ID> (see below for a description of command ID).

The workings of the client and server are described below:

FTP Server (myftpserver program) - The server program takes two command line parameters, which are the port numbers where the server will wait on (one for normal commands and another for the “terminate” command). The port for normal commands are henceforth referred to as “nport” and the terminate port is referred to as “tport”. Once the myftpserver program is invoked, it should create two threads. The first thread will create a socket on the first port number and wait for incoming clients. The second thread will create a socket on the second port and wait for incoming “terminate” commands.

When a client connects to the normal port, the server will spawn off a new thread to handle the client commands. However, when the serve gets a “get” or a “put” command, it will immediately send back a command ID to the client. This is for the clients to use when they need to terminate a currently-running command. Furthermore, the threads executing the “get” and “put” commands, will periodically (after transferring 1000 bytes) check the status of the command to see if the client needs the command to be terminated. If so, it will stop transferring data, delete any files that were created and will be ready to execute more commands.

Note that there might be several normal threads executing concurrency. The server can handle the consistency issues arising out of such concurrency. For example, if two put requests arrive concurrently for the same file, one request should be completed before the other starts.

When a client connects to the “tport”, the server accepts the terminate command. The command will identify (using the command-ID) which command needs to be terminated. It will set the status of that command to “terminate” so that the thread executing that command will notice it and gracefully terminate.

FTP Client: The ftp client program takes three command line parameters the machine name where the server resides, the normal port number, and the terminate port number. Once the client starts up, it display a prompt “mytftp>”. It then accept and execute commands. However, if any command is appended with a “&” sign (e.g., get file1.txt &), then this command should be executed in a separate thread. The main thread continue to wait for more commands (i.e., it will not be blocked for the other threads to complete). For “get” and “put” commands, the client displays the command-ID received from the server. When the user enters a terminate command, the client uses the tport to relay the command to the server. The client also cleans up any files that were created as a result of commands that were terminated.


Points to note:

1. It assumes that each client has only one connection to the server through the normal port (this will avoid complications with respect to changing directories in one thread while other thread is operating in another directory). However, note that different clients can still concurrently connect to the server.
 
2. It assumes that the user uses the correct syntax when entering various commands.

3. There is no user logins.

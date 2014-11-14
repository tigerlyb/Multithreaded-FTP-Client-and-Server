#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>

#define SIZE	1024

pthread_t n_thread[SIZE];

int t_id = 0;
char *fdict[SIZE];
int sdict[SIZE] = {0};
int get_id_alive[SIZE] = {0};
int put_id_alive[SIZE] = {0};
char *mutex_dict[SIZE]={NULL};

void socket_connect_nport (int port);
void socket_connect_tport (int port);
void n_thread_process (int socket);
void t_thread_process (int socket);
void echo (int sid, char *str);
void get (int sid, char *str);
void put (int sid, char *str);
void delete (int sid, char *str);
void ls (int sid, char *str);
void cd (int sid, char *str);
void s_mkdir (int sid, char *str);
void pwd (int sid);
void quit ();
void terminate (char *str);
int get_sid(int *dict, int tid);
int file_in_use(char **mdict, char *fname);

int main (int argc, char **argv) {
	int nport, tport;
	pthread_t nport_thread;
	pthread_t tport_thread;
	
	// take in user specified port numbers from command line argument
	if (argc != 3) {
		printf("Please enter two port numbers.\n");
	} else {
		nport = atoi(argv[1]);
		tport = atoi(argv[2]);
		
		if (nport > 1024 && nport < 65535 && tport > 1024 && tport < 65535) {
			pthread_create(&nport_thread, NULL, (void *) &socket_connect_nport, (int *) nport);
			pthread_create(&tport_thread, NULL, (void *) &socket_connect_tport, (int *) tport);
			pthread_join(nport_thread, NULL);
			pthread_join(tport_thread, NULL);
			
		} else {
			printf("Please enter the port number between 1025 and 65534.\n");
		}
	}
	
	return 0;
}

void socket_connect_nport (int port) {
	int sockid, s;
	char buf[SIZE];
	struct sockaddr_in server;
	
	printf("nport#: %d\n", port);
		
	// set up socket
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		printf("Cannot create socket.\n");
		exit(errno);
	}
	
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	
	if (bind(sockid, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("Cannot bind.\n");
		exit(errno);
	}
	
	listen(sockid, SIZE);
	
	while ((s = accept(sockid, (struct sockaddr *) NULL, NULL)) > 0) {
		sdict[s] = t_id;
		pthread_create(&n_thread[t_id], NULL, (void *) &n_thread_process, (int *) s);
		t_id++;
	}
}

void socket_connect_tport (int port) {
	int sockid, s;
	char buf[SIZE];
	struct sockaddr_in server;
	printf("tport#: %d\n", port);
		
	// set up socket
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		printf("Cannot create socket.\n");
		exit(errno);
	}
	
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	
	if (bind(sockid, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("Cannot bind.\n");
		exit(errno);
	}
	
	listen(sockid, 10);
	
	while ((s = accept(sockid, (struct sockaddr *) NULL, NULL)) > 0) {
		pthread_t t_thread;
		pthread_create(&t_thread, NULL, (void *) &t_thread_process, (int *) s);
	}
}

void n_thread_process (int socket) {
	char buf[SIZE];
	bzero(buf, SIZE);
	read(socket, buf, SIZE);		
	echo(socket, buf);	
	
}

void t_thread_process (int socket) {
	char buf[SIZE];
	bzero(buf, SIZE);
	read(socket, buf, SIZE);		
	terminate(buf);	
}

void echo (int sid, char *str) {
	int select;
	char *e = "Invalid Command.\n\n";
	
	if (strncmp(str, "get", 3) == 0)
		select = 1;
	else if (strncmp(str, "put", 3) == 0)
		select = 2;
	else if (strncmp(str, "delete", 6) == 0)
		select = 3;
	else if (strncmp(str, "ls", 2) == 0)
		select = 4;
	else if (strncmp(str, "cd", 2) == 0)
		select = 5;
	else if (strncmp(str, "mkdir", 5) == 0)
		select = 6;
	else if (strncmp(str, "pwd", 3) == 0)
		select = 7;
	else if (strncmp(str, "quit", 4) == 0)
		select = 8;
	else
		select = 0;

	switch(select) {
		case 1:
			get(sid, str);
			break;
		case 2:
			put(sid, str);
			break;
		case 3:
			delete(sid, str);
			break;
		case 4:
			ls(sid, str);
			break;
		case 5:
			cd(sid, str);
			break;
		case 6:
			s_mkdir(sid, str);
			break;
		case 7:
			pwd(sid);
			break;
		case 8:
			quit();
			break;
		default:
			printf("Invalid Command.\n\n");
			write(sid, e, strlen(e));
	}
}

void terminate (char *str) {
	int tid = atoi(str);
	if (put_id_alive[tid] == 1) {
		pthread_cancel(n_thread[tid]);
		mutex_dict[tid] = NULL;
		remove(fdict[tid]);
		printf("Put command %d is terminated.\n", tid);
		printf("fdict[%d]: %s is released.\n",tid, fdict[tid]);
		printf("%s is removed.\n\n", fdict[tid]);
		put_id_alive[tid] = 0;
	} else if (get_id_alive[tid] == 1) {
		pthread_cancel(n_thread[tid]);
		printf("Get command %d is terminated.\n", tid);
		get_id_alive[tid] = 0;
	} else {
		printf("Command %d isn't running in the server. No termination needed.\n\n", tid);
	}
}

void get (int sid, char *str) {
	char *ferror = "No such file";	// error message
	char buf[SIZE];
	char f[100];
	int i;
	char t_id_string[10];

	bzero(f, sizeof(f));
	bzero(buf, sizeof(buf));
	
	int tid = sdict[sid];
	sprintf(t_id_string, "%d", tid);
	get_id_alive[tid] = 1;
	
	// get the file name
	for (i = 4; i < strlen(str)-1; i++) {
		f[i-4] = str[i];	
	}
	
	FILE *file = fopen(f, "rb");
	if (file == NULL) {
		printf("No such file: %s\n\n", f);
		write(sid, ferror, strlen(ferror));
		get_id_alive[tid] = 0;
		return;
	}

	write(sid, t_id_string, strlen(t_id_string));
	
	int ret = file_in_use(mutex_dict, f);
	while(ret == 1){
		ret = file_in_use(mutex_dict, f);
	}

	read(sid, buf, sizeof(buf)); // read the sending signal
	printf("Sending file %s.\n", buf);
	bzero(buf, sizeof(buf));

	while(fread(buf, 1, 1000, file) > 0) {	// read file content to the buffer
		write(sid, buf, strlen(buf));		// write the buffer to the client
		if(strlen(buf) < 1000) {
			printf("All bytes of %s has been sent. Command ID is %d.\n", f, tid);
		} else {
			printf("1000 bytes of %s has been sent. Command ID is %d.\n", f, tid);
			printf("Checking termination status...\n");
		}
		bzero(buf, sizeof(buf));
		sleep(2);
	}
	
	fclose(file);
	close(sid);
	get_id_alive[tid] = 0;
}

void put (int sid, char *str) {
	
	char f[100];	// file name
	char buf[SIZE];
	int i;
	char t_id_string[10];
	
	int tid = sdict[sid];
	sprintf(t_id_string, "%d", tid);
	put_id_alive[tid] = 1;
	
	write(sid, t_id_string, strlen(t_id_string));
	
	bzero(f, sizeof(f));
	bzero(buf, sizeof(buf));
	
	// get the file name
	for (i = 4; i < strlen(str) - 1; i++) {
		f[i-4] = str[i];
	}
	
	fdict[tid] = f;
	int ret = file_in_use(mutex_dict, f);
	while(ret == 1){
		ret = file_in_use(mutex_dict, f);
	}
	
	mutex_dict[tid] = f;
	printf("fdict[%d]: %s is locked.\n",tid, fdict[tid]);
	
	FILE *file = fopen(f, "wb");
	if (file == NULL) {
		printf("Cannot write the file %s\n", f);
		put_id_alive[tid] = 0;
		return;
	}
	
	while (read(sid, buf, 1000) > 0) {
		fwrite(buf, sizeof(char), strlen(buf), file);
		if(strlen(buf) < 1000) {
			printf("All bytes of %s has been received. Command ID is %d.\n", f, tid);
		} else {
			printf("1000 bytes of %s has been received. Command ID is %d.\n", f, tid);
			printf("Checking termination status...\n");
		}
		bzero(buf, sizeof(buf));
		sleep(2);
	}
	
	fclose(file);
	printf("Get file %s from client!\n", f);
	
	mutex_dict[tid] = NULL;
	printf("fdict[%d]: %s is released.\n\n",tid, fdict[tid]);
	
	put_id_alive[tid] = 0;
}

void delete (int sid, char *str) {
	char f[100];
	int i;
	char *fail_delete = "No such file or error deleting file.\n";
	char *success_delete = "File successfully deleted.\n";
	
	bzero(f, sizeof(f));
	
	// get the file name
	for (i = 7; i < strlen(str) - 1; i++) {
		f[i-7] = str[i];
	}
	
	if (remove(f) != 0) {
		printf("%s: %s\n\n", f, fail_delete);
		write(sid, fail_delete, strlen(fail_delete));
	} else {
		printf("%s: %s\n\n", f, success_delete);
		write(sid, success_delete, strlen(success_delete));
	}
}

void ls (int sid, char *str) {
	FILE * fp;
	char buffer[SIZE];
	char ret[SIZE] = "";

	fp = popen(str, "r");
	while(fgets(buffer,sizeof(buffer),fp) != NULL){
		strcat(ret, buffer);
	}
	write(sid, ret, sizeof(ret));
	pclose(fp);
}

void cd (int sid, char *str) {
	char f[100];
	int i;

	char *fail_cd = "Failed to change dir.\n";
	char *success_cd = "Dir successfully changed.\n";
	
	bzero(f, sizeof(f));
	for (i = 3; i < strlen(str) - 1; i++) {
		f[i-3] = str[i];
	}
	if(chdir(f) != 0){
		printf("%s: %s\n\n", f, fail_cd);
		write(sid, fail_cd, strlen(fail_cd));
	}
	else{
		printf("%s: %s\n\n", f, success_cd);
		write(sid, success_cd, strlen(success_cd));
	}
}

void s_mkdir (int sid, char *str) {
	char f[100];
	int i;
	char *fail_mkdir = "Failed to create dir.\n";
	char *success_mkdir = "Dir successfully created.\n";
	
	bzero(f, sizeof(f));
	for (i = 6; i < strlen(str) - 1; i++) {
		f[i-6] = str[i];
	}
	if(mkdir(f, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH) < 0){
		printf("%s: %s\n\n", f, fail_mkdir);
		write(sid, fail_mkdir, strlen(fail_mkdir));
	}
	else{
		printf("%s: %s\n\n", f, success_mkdir);
		write(sid, success_mkdir, strlen(success_mkdir));
	}
}

void pwd (int sid) {
	FILE * fp;
	char buffer[SIZE];
	system("pwd");
	fp = popen("pwd", "r");
	fgets(buffer,sizeof(buffer),fp);
	write(sid, buffer, sizeof(buffer));
	pclose(fp);
}

void quit () {
	printf("Client disconnected.\n\n");
}

int get_sid(int *dict, int tid){
	int i = 0;
	for(i = 0; i < SIZE-1; i++){
		if(dict[i] == tid){
			return i;
		}
	}
	return -1;
}

int file_in_use(char **mdict, char *fname){
	int i = 0;
	for(i = 0; i < SIZE-1; i++){
		if((mdict[i] != NULL) && (strcmp(mdict[i], fname) == 0)){
			return 1;
		}
	}
	return 0;
}

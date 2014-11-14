#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>
#include <pthread.h>


#define SIZE	1024

pthread_t main_thread;
pthread_t put_thread[SIZE];
pthread_t get_thread[SIZE];

typedef struct str_thdata
{
	char message[SIZE];
} thdata;

int nport, tport;
char *serverIP;

int put_id = 0;
int get_id = 0;
char *get_fdict[SIZE];
int get_sdict[SIZE] = {0};
int put_sdict[SIZE] = {0};
int get_id_alive[SIZE] = {0};
int put_id_alive[SIZE] = {0};
char *get_mutex_dict[SIZE]={NULL};

void socket_connect ();
void socket_connect_nport (void *ptr);
void socket_connect_tport (char *command_id);
void echo (int sid, char *str, char *r_str);
void get (int sid, char *str, char *r_str);
void get_extend (int sid, char *str, char *r_str);
void put (int sid, char *str, char *r_str);
void put_extend (int sid, char *str, char *r_str);
void quit (int sid, char *str);
void terminate (char *str);
int get_tid(int *dict, int sid);
int file_in_use(char **mdict, char *fname);

int main (int argc, char **argv) {

	// take in user specified port number from command line argument
	if (argc != 4) {
		printf("Please enter the server address and two port numbers.\n");
	} else {
		serverIP = argv[1];
		nport = atoi(argv[2]);
		tport = atoi(argv[3]);
		
		pthread_create(&main_thread, NULL, (void *) &socket_connect, NULL);
		pthread_join(main_thread, NULL);	
	}
	return 0;
}

void socket_connect () {
	int sockid, select;
	char reply[SIZE];
	char buf[SIZE];
	char id_string[10];
	struct sockaddr_in server;
	thdata data;
	
	while (1) {
		bzero(buf, sizeof(buf));
		bzero(data.message, sizeof(data.message));
		bzero(id_string, sizeof(id_string));
		
		printf("\nmyftp> ");
		fgets(buf, sizeof(buf), stdin);
		
		strcpy(data.message, buf);
		bzero(reply, sizeof(reply));
		
		if (strncmp(buf, "terminate", 9) == 0) {
			int i;
			for (i = 0; i <= (strlen(buf) - 9); i++) {
				id_string[i] = buf[i + 10];
			}
			pthread_t terminate_thread;
			pthread_create(&terminate_thread, NULL, (void *) &socket_connect_tport,  &id_string);
			sleep(1);
		} else if (strncmp(buf, "&get", 4) == 0) {
			pthread_create(&get_thread[get_id], NULL, (void *) &socket_connect_nport, (void *) &data);
			sleep(1);
		} else if (strncmp(buf, "&put", 4) == 0) {
			pthread_create(&put_thread[put_id], NULL, (void *) &socket_connect_nport, (void *) &data);
			sleep(1);			
		} else {
			// set up socket
			sockid = socket(AF_INET, SOCK_STREAM, 0);
			if (sockid < 0) {
				printf("Cannot create socket.\n");
				exit(errno);
			}
			
			bzero(&server, sizeof(server));
			server.sin_family = AF_INET;
			server.sin_addr.s_addr = inet_addr(serverIP);
			server.sin_port = htons(nport);
			
			if (connect(sockid, (struct sockaddr *)&server, sizeof(server)) < 0) {
				printf("Cannot connect to the server.\n");
				exit(errno);
			}
		
			if (strncmp(buf, "get", 3) == 0)
				select = 1;	// get command
			else if (strncmp(buf, "put", 3) == 0)
				select = 2;	// put command
			else if (strncmp(buf, "quit", 4) == 0)
				select = 3;	// quit command
			else 
				select = 0;	// delete, ls, cd, mkdir, pwd and other invalid command	

			switch(select) {
				case 1:
					get (sockid, buf, reply);
					break;
				case 2:
					put (sockid, buf, reply);
					break;
				case 3:
					quit (sockid, buf);
					break;
				default:
					echo (sockid, buf, reply);
			}
		}
	}
}

void socket_connect_nport (void *ptr) {
	thdata *data;
	data = (thdata *) ptr;
	
	int sockid;
	int select;
	char reply[SIZE];
	struct sockaddr_in server;
	
	// set up socket
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		printf("Cannot create socket.\n");
		exit(errno);
	}
	
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(serverIP);
	server.sin_port = htons(nport);
	
	if (connect(sockid, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("Cannot connect to the server.\n");
		exit(errno);
	}

	if (strncmp(data->message, "&get", 4) == 0)
		select = 1;	// &get command
	else
		select = 2;	// &put command
	
	bzero(reply, sizeof(reply));
			
	switch(select) {
		case 1:
			get_extend (sockid, data->message, reply);
			break;
		case 2:
			put_extend (sockid, data->message, reply);
			break;
	}
	
	pthread_exit(NULL);
}

void socket_connect_tport (char *command_id) {
	
	int sockid;
	char buf[SIZE], reply[SIZE];
	struct sockaddr_in server;
	
	// set up socket
	sockid = socket(AF_INET, SOCK_STREAM, 0);
	if (sockid < 0) {
		printf("Cannot create socket.\n");
		exit(errno);
	}
	
	bzero(&server, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = inet_addr(serverIP);
	server.sin_port = htons(tport);
	
	if (connect(sockid, (struct sockaddr *)&server, sizeof(server)) < 0) {
		printf("Cannot connect to the server.\n");
		exit(errno);
	}
			
	printf("Command_id: %s", command_id);
	terminate (command_id);
	write(sockid, command_id, strlen(command_id));
	
	pthread_exit(NULL);
}

void terminate (char *str) {
	int cid = atoi(str);
	int gid = get_tid(get_sdict, cid);
	int pid = get_tid(put_sdict, cid);
	if (get_id_alive[gid] == 1) {
		pthread_cancel(get_thread[gid]);
		get_mutex_dict[gid] = NULL;
		remove(get_fdict[gid]);
		printf("Get command %d is terminated.\n", cid);
		printf("%s is removed.\n\n", get_fdict[gid]);
		get_id_alive[gid] = 0;
	} else if (put_id_alive[pid] == 1) {
		pthread_cancel(put_thread[pid]);
		printf("Put command %d is terminated.\n", cid);
		put_id_alive[pid] == 0;
	} else {
		printf("Command %d isn't running in the client. No termination needed.\n\n", cid);
	}
}

void echo (int sid, char *str, char *r_str) {
	write(sid, str, strlen(str));
	read(sid, r_str, SIZE);
	printf("%s", r_str);	
	close(sid);
}

void get (int sid, char *str, char *r_str) {
	char *nofile = "No such file";
	char *ferror = "Cannot write the file.\n";
	char *signal = "signal";
	char f[100];
	int i;
	
	bzero(f, sizeof(f));
	
	// get the file name
	for (i = 4; i < strlen(str) - 1; i++) {
		f[i-4] = str[i];
	}
	
	write(sid, str, strlen(str));		// write the command to the server
	read(sid, r_str, SIZE);				
	
	if (strncmp(r_str, nofile, 12) != 0) {	
		int c_id = atoi(r_str);
		printf("Command ID: %s\n", r_str);
		bzero(r_str, strlen(r_str));
		write(sid, signal, strlen(signal));	// send a signal to server to start getting file
		
		FILE *file = fopen(f, "wb");	
		
		if (file == NULL) {
			printf("%s", ferror);
			close(sid);
			return;
		}
		
		while (read(sid, r_str, 1000) > 0) {
			fwrite(r_str, sizeof(char), strlen(r_str), file);
			if(strlen(r_str) < 1000) {
				printf("All bytes of %s has been received. Command ID is %d.\n", f, c_id);
			} else {
				printf("1000 bytes of %s has been received. Command ID is %d.\n", f, c_id);
			}
			bzero(r_str, strlen(r_str));
		}
		
		fclose(file);
		printf("Get file %s from server!\n", f);
	} else {
		printf("%s: %s", r_str, f);
	}
	close(sid);
}

void get_extend (int sid, char *str, char *r_str) {
	int get_id_tmp = get_id;
	get_id_alive[get_id_tmp] = 1;
	get_id++;
	
	char *nofile = "No such file";
	char *ferror = "Cannot write the file.\n";
	char *signal = "signal";
	char buf[100];
	char f[100];
	int i;
	
	bzero(buf, sizeof(buf));
	bzero(f, sizeof(f));
	
	// get the file name
	for (i = 5; i < strlen(str) - 1; i++) {
		f[i-5] = str[i];
	}
	
	// set get command without & sign to send
	for (i = 1; i < strlen(str); i++) {
		buf[i-1] = str[i];
	}
	
	write(sid, buf, strlen(buf));		// write the command to the server
	read(sid, r_str, SIZE);				
	
	if (strncmp(r_str, nofile, 12) != 0) {
		
		int c_id = atoi(r_str);
		get_sdict[get_id_tmp] = c_id;
		
		printf("Command ID: %s\n", r_str);
		bzero(r_str, strlen(r_str));
				
		get_fdict[get_id_tmp] = f;
		int ret = file_in_use(get_mutex_dict, f);
		
		while(ret == 1){
			ret = file_in_use(get_mutex_dict, f);
		}
		
		get_mutex_dict[get_id_tmp] = f;
		
		FILE *file = fopen(f, "wb");	
		if (file == NULL) {
			printf("%s", ferror);
			close(sid);
			get_id_alive[get_id_tmp] = 0;
			return;
		}
		
		write(sid, signal, strlen(signal));	// send a signal to server to start getting file
		while (read(sid, r_str, 1000) > 0) {
			fwrite(r_str, sizeof(char), strlen(r_str), file);
			bzero(r_str, strlen(r_str));
		}
		
		fclose(file);
		get_mutex_dict[get_id_tmp] = NULL;	
		
	} else {
		printf("%s: %s", r_str, f);
	}
	close(sid);
	get_id_alive[get_id_tmp] = 0;
}

void put (int sid, char *str, char *r_str) {
	char buf[SIZE];
	char f[100];
	int i;
	
	bzero(buf, sizeof(buf));
	bzero(f, sizeof(f));
	
	// get the file name
	for (i = 4; i < strlen(str) - 1; i++) {
		f[i-4] = str[i];
	}
		
	FILE *file = fopen(f, "rb");
	if (file == NULL) {
		printf("No such file to put: %s\n", f);
		return;
	}
	
	write(sid, str, strlen(str));		// write the command to the server
	read(sid, r_str, SIZE);				// read the command_id from the server
	int c_id = atoi(r_str);
	printf("Command ID: %s\n", r_str);
	
	bzero(r_str, strlen(r_str));
	
	while(fread(buf, 1, 1000, file) > 0) {	// read file content to the buffer
		write(sid, buf, strlen(buf));		// write the buffer to the server
		if(strlen(buf) < 1000) {
			printf("All bytes of %s has been sent. Command ID is %d.\n", f, c_id);
		} else {
			printf("1000 bytes of %s has been sent. Command ID is %d.\n", f, c_id);
		}
		bzero(buf, sizeof(buf));
		sleep(2);
	}
	
	fclose(file);
	close(sid);
}

void put_extend (int sid, char *str, char *r_str) {
	int put_id_tmp = put_id;
	put_id++;
	put_id_alive[put_id_tmp] = 1;

	char buf[SIZE];
	char f[100];
	int i;
	
	bzero(buf, sizeof(buf));
	bzero(f, sizeof(f));
	
	// get the file name
	for (i = 5; i < strlen(str) - 1; i++) {
		f[i-5] = str[i];
	}
	
	FILE *file = fopen(f, "rb");
	if (file == NULL) {
		printf("No such file to put: %s\n", f);
		put_id_alive[put_id_tmp] = 0;
		return;
	}
	
	// set put command without & sign to send
	for (i = 1; i < strlen(str); i++) {
		buf[i-1] = str[i];
	}

	write(sid, buf, strlen(buf));		// write the command to the server
	read(sid, r_str, SIZE);				// read the command_id from the server
	int c_id = atoi(r_str);
	printf("Command ID: %s\n", r_str);
	put_sdict[put_id_tmp] = c_id;
	bzero(r_str, strlen(r_str));
	bzero(buf, sizeof(buf));
	
	while(fread(buf, 1, 1000, file) > 0) {	// read file content to the buffer
		write(sid, buf, strlen(buf));		// write the buffer to the server
		bzero(buf, sizeof(buf));
		sleep(2);
	}	
	
	fclose(file);
	close(sid);
	put_id_alive[put_id_tmp] = 0;
}

void quit (int sid, char *str) {
	write(sid, str, strlen(str));
	printf("End the FTP session.\n");
	exit(1);
}

int get_tid(int *dict, int cid){
	int i = 0;
	for(i = 0; i < SIZE-1; i++){
		if(dict[i] == cid){
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

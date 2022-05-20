#include "server.h"
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

int main(int argc, char** argv){
	// Build socket
	build_socket(argc, argv);
	
	return 0;
}

// This function build a socket
// Refernce: Line 28-78 refers to Practical week9-open source code-server.c
// Link: https://gitlab.eng.unimelb.edu.au/comp30023-2022-projects/practicals.git 
void build_socket(int input_argc, char** input_argv) {
	int argc = input_argc;
	char** argv = input_argv;
	int sockfd, newsockfd, re, s;
	struct addrinfo hints, *res;
	struct sockaddr_storage client_addr;
	socklen_t client_addr_size;
	
	// Allow 4 inputs,  protocol number,  port number, string path and root.
	if (argc < 4) {
		fprintf(stderr, "ERROR, no port provided\n");
		exit(EXIT_FAILURE);
	}

	// Create address we're going to listen on (with given port number)
	memset(&hints, 0, sizeof hints);
	
	// Setup protocal IPV4/IPV6 
	if(strcmp(argv[1], "4")==0){
		hints.ai_family = AF_INET;
	}else if(strcmp(argv[1], "6")==0){
		hints.ai_family = AF_INET6;
	}  

	hints.ai_socktype = SOCK_STREAM; // TCP
	hints.ai_flags = AI_PASSIVE;     // for bind, listen, accept
	// node (NULL means any interface), service (port), hints, res
	s = getaddrinfo(NULL, argv[2], &hints, &res);
	if (s != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}

	// Create socket
	sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd < 0) {
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Reuse port if possible
	re = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &re, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}
	// Bind address to the socket
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) < 0) {
		perror("bind");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(res);

	// Listen on socket - means we're ready to accept connections,
	// incoming connection requests will be queued, man 3 listen
	if (listen(sockfd, 5) < 0) {
		perror("listen");
		exit(EXIT_FAILURE);
	}

	// Accept a connection - blocks until a connection is ready to be accepted
	// Get back a new file descriptor to communicate on
	// Make socket keep running
	pthread_t tid;
	while(TRUE){
		// Build connection
		client_addr_size = sizeof client_addr;
		newsockfd = accept(sockfd, (struct sockaddr*)&client_addr, &client_addr_size);
		char *root = argv[3];
		multi_thread_t *new_thread = malloc(sizeof(multi_thread_t));
		new_thread->newsockfd =newsockfd;
		new_thread->root =  root;
		// Set up a new thread
		pthread_create(&tid, NULL, build_multi_thread, (void*)new_thread); 
	}
	close(sockfd);
}

// This function set up multi-thread feature
void *build_multi_thread(void *data){
	multi_thread_t *multi_thread = (multi_thread_t *)data;
	int newsockfd = multi_thread->newsockfd;
	char *root = multi_thread->root;
	
	// Build transmission
	build_transmission(newsockfd, root);
	
	// Close current connection
	close(newsockfd);	
	// Free memory
	free(data);

	return NULL;
}

// This function transmit data between client and server
void build_transmission(int input_newsockfd,char *input_root){
	int newsockfd = input_newsockfd;
	char buffer[4*ONE_KILO_BYTE];;
	int n = 0;
	char *root = input_root;

	if (newsockfd < 0) {
		perror("accept");
		exit(EXIT_FAILURE);
	}
	
	// Read characters from the connection, then process them
	char current_read[2*ONE_KILO_BYTE];

	// Before "\r\n\r\n" is detected, keep receiving the path
	while((n = read(newsockfd, current_read, 2*ONE_KILO_BYTE))>0){
		strcat(buffer, current_read);
		if(strstr(buffer, "\r\n\r\n")){
			break;
		}
	} 

	if (n < 0) {
		perror("read");
		exit(EXIT_FAILURE);
	}
	// Null-terminate string
	buffer[n] = '\0';
		
	// Extract data from buffer
	char* sperator = " ";
	char* half_path = strtok(buffer, sperator);
	//Continue from the last stop point
	half_path = strtok(NULL, sperator);

	// Detect the full path
	// Root is argv[3]
	char* full_path = malloc(strlen(half_path) + strlen(root) + 1);
	strcpy(full_path, root); //Now full path includes the root.
	strcat(full_path, half_path);
	
	// Send responce to client
	send_response(full_path, half_path, newsockfd);
}

// This function send required data back to client, if it exists
void send_response(char *f_path, char *h_path, int n_sockfd){
	char *full_path = f_path;
	char *half_path = h_path;
	int newsockfd = n_sockfd;

	// Open file
	FILE* fp = fopen(full_path, "r");
	
	// Check if path include ".."
	int is_dot_exit = 0;
	char* substr = NULL;
	char* compare_str = "..";	
	substr = strstr(half_path, compare_str);
	// If ".." exists		 
	if(substr){
		is_dot_exit = 1;
	} 

	// If ".." exits in path
	int fail_msg_len = 0;
	int sucesses_msg_len = 0;
	if (is_dot_exit){
		fail_msg_len = strlen("HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n");
		write(newsockfd, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n", fail_msg_len);
	} 
	// Check if the target path is in the server
	// If file exists, do following steps
	else if (fp != NULL){
		sucesses_msg_len = strlen("HTTP/1.0 200 OK\r\n");
		write(newsockfd, "HTTP/1.0 200 OK\r\n", sucesses_msg_len);
		char content_type[MAX_BYTES];
		char content[2*MAX_BYTES];

		// Find the content type
		char* current_path = NULL;
		char* previous_path = half_path; 	
		// Init current path
		current_path = strtok(half_path, ".");
		// Loop the path and find the last part
		while(current_path != NULL){
			previous_path = current_path;
			current_path = strtok(NULL, ".");
		}

		// Assign content type
		if (strcmp(previous_path, "css")==0){
			strcpy(content_type, "text/css");
		}else if(strcmp(previous_path, "js")==0){
			strcpy(content_type, "text/js");
		}else if(strcmp(previous_path, "html")==0){
			strcpy(content_type, "text/html");
		}else if(strcmp(previous_path, "jpg")==0){
			strcpy(content_type, "image/jpeg");
		} else{
			strcpy(content_type, "application/octet-stream");
		}

		// Concate msg together, then send it to client.
		sprintf(content, "Content-Type: %s\r\n\r\n", content_type);
		sucesses_msg_len = strlen(content);
		write(newsockfd, content, sucesses_msg_len);

		// Send file data to client
		char data[4*ONE_KILO_BYTE];
		int data_len = 0;
		int is_write_sucesses = 0;

		while((data_len = fread(data, sizeof(char), 4*ONE_KILO_BYTE, fp))!=0){
			is_write_sucesses = write(newsockfd, data, data_len);
			if(is_write_sucesses<0){
				perror("write");
				exit(EXIT_FAILURE);
			}
		}

		// Close file 
		fclose(fp);
	}
	// If path doesn't exit.
	else{
		fail_msg_len = strlen("HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n");
		write(newsockfd, "HTTP/1.0 404 Not Found\r\nContent-Type: text/html\r\n\r\n", fail_msg_len);
	}

	// Free memory
	free(f_path);
}
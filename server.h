#define _POSIX_C_SOURCE 200112L
#define MAX_BYTES 256
#define ONE_KILO_BYTE 1024
#define TRUE 1
#define FALSE 0
#define IMPLEMENTS_IPV6
#define MULTITHREADED


typedef struct{
	int newsockfd;
	char *root;
}multi_thread_t;


void *build_multi_thread(void *data);
void build_socket(int input_argc, char** input_argv);
void send_response(char *full_path, char *half_path, int newsockfd);
void build_transmission(int input_newsockfd,char *input_root);

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400
#define NTHREADS 4
#define SBUFSIZE 16

/* You won't lose style points for including this long line in your code */
static char *user_agent_hdr = "Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3";
static char *http_version_hdr = "HTTP/1.0";
int sock;
struct sockaddr_in addr;
sbuf_t sbuf;

struct http_header;
struct http_header{
	char* name;
	char* value;
	struct http_header* next;
};
typedef struct http_header http_header;

typedef struct{
	char* type;
	char* url;
	char* version;
	http_header* next;
} http_request;


/******** Conversion functions**********/

http_request* string_to_http(char* buffer){
	http_request* request = malloc(sizeof(http_request));
	char* buffer_rest = buffer;
	char* line = strtok_r(strdup(buffer), "\r\n", &buffer_rest);
	char* line_rest = line;

	char* type = strtok_r(strdup(line), " ", &line_rest);
	request->type = type;//TODO need to malloc?

	char* url = strtok_r(NULL, " ", &line_rest);
	request->url = url;

	char* version = strtok_r(NULL, " ", &line_rest);
	request->version = version;

	//TODO check if anything else in first line?

	//printf("all\n%s\n", line);

	http_header* current = NULL;

	while (line = strtok_r(NULL, "\r\n", &buffer_rest)){
		line_rest = line;

		char* name = strtok_r(strdup(line), ": ", &line_rest);
		//printf("%s:%s\n", name, line_rest);
		http_header* header = malloc(sizeof(http_header));
		header->name = name;
		header->value = line_rest;//TODO malloc string?
		header->next = NULL;
		if (current == NULL){
			request->next = header;
			current = header;
		}
		else{
			current->next = header;
			current = header;
		}
		//printf("%s\n", line);
	}

	//printf("%s\n", "PARSED");
	return request;
}

char* http_to_string(http_request* request){
	//TODO need to malloc?
	char* buffer = malloc(MAX_OBJECT_SIZE);
	strcpy(buffer, request->type);
	strcat(buffer, " ");
	strcat(buffer, request->url);
	strcat(buffer, " ");
	strcat(buffer, request->version);
	strcat(buffer, "\r\n");
	http_header* current = request->next;
	while(current){
		strcat(buffer, current->name);
		strcat(buffer, ":");//TODO add space?
		strcat(buffer, current->value);
		strcat(buffer, "\r\n");
		current = current->next;
	}
	strcat(buffer, "\r\n");
	return buffer;//TODO free
}


/******** HTTP Request functions ************/

void free_request(http_request* request){

}

char* get_header(http_request* request, char* name){
	http_header* current = request->next;
	while (current){
		if (strcmp(current->name, name) == 0){
			return current->value;
		}
		current = current->next;
	}
	return NULL;
}

void add_header(http_request* request, char* name, char* value){
	//TODO does it matter the order of headers?
	http_header* new_header = malloc(sizeof(http_header));
	new_header->name = name;
	new_header->value = value;

	http_header* next = request->next;
	new_header->next = next;
	request->next = new_header;
}

void remove_header(http_request* request, char* name){
	http_header* past = NULL;
	http_header* current = request->next;
	while (current){
		if (strcmp(current->name, name) == 0){
			http_header* tmp = current;
			current = current->next;
			if (past){
				past->next = current;
			}
			else{
				request->next = current;
			}
			free(tmp);
		}
		else{
			past = current;
			current = current->next;
		}
	}
}

void replace_http_version(http_request* request){
	request->version = http_version_hdr;
}

void replace_user_agent(http_request* request){
	remove_header(request, "User-Agent");
	add_header(request, "User-Agent", user_agent_hdr);
}

char* get_host_from_url(char* url){
	char* url_rest = url;
	char* host = strtok_r(url, "/", &url_rest);
	printf("%s\n", "hoststststts");
	printf("%s\n", url);
	printf("%s\n", host);
	printf("%s\n", url_rest);
	return host;
}

char* get_content_from_url(char* url){
	char* url_rest = url;
	char* host = strtok_r(url, "//", &url_rest);
	host = strtok_r(NULL, "/", &url_rest);
	char* content = malloc(sizeof(url));
	strcpy(content, "/");
	strcat(content, url_rest);
	return content;
}

// Make all the desired changes to the request
void proxy_strip_request(http_request* request){
	replace_http_version(request);
	replace_user_agent(request);
	remove_header(request, "Connection");
	add_header(request, "Connection", "close");
	remove_header(request, "Proxy-Connection");
	add_header(request, "Proxy-Connection", "close");
	if (!get_header(request, "Host")){
		char* host = get_host_from_url(request->url);
		add_header(request, "Host", host);
	}
	if (request->url[0] != '/'){
		char* content = get_content_from_url(request->url);
		request->url = content;
	}
}

char *trimwhitespace(char *str){
	char *end;

	// Trim leading space
	while(isspace((unsigned char)*str)) str++;

	if(*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace((unsigned char)*end)) end--;

	// Write new null terminator character
	end[1] = '\0';
	return str;
}

char* make_request(char* host, char* request){
	char* host_rest = host;
	char* server = strtok_r(host, ":", &host_rest);
	char* port = NULL;
	if (host_rest != NULL){
		port = host_rest;
	}
	server = trimwhitespace(server);
	port = trimwhitespace(port);
	
	rio_t rio;
	int request_sock = Open_clientfd(server, port);
	
	char* response = malloc(MAX_OBJECT_SIZE);
	memset(response, 0, sizeof(response));
	Rio_readinitb(&rio, request_sock);
	Rio_writen(request_sock, request, strlen(request));
	//send(request_sock, request, strlen(request), 0);
	char* response_line[MAXLINE];
	int header_size = 0;
	//Read headers
	while (Rio_readlineb(&rio, response_line, MAXLINE) > 0){
		if (strcmp(response_line, "\r\n") == 0){
			break;
		}
		header_size += strlen(response_line);
		//printf("{%s}", response_line);
		strcat(response, response_line);
	}
	strcat(response, "\r\n");//TODO need this?
	header_size += 2;
	//printf("\nBEGIN<%s>END\n", response);

	//Read body
	char body[MAX_OBJECT_SIZE];
	Rio_readnb(&rio, body, (MAX_OBJECT_SIZE - header_size));
	memcpy(response+header_size, body, (MAX_OBJECT_SIZE - header_size));
	//recv(request_sock, response, MAX_OBJECT_SIZE, 0);
	
	
	close(request_sock);

	return response;
}

void proxy_thread(int csock){	
	char* buffer = malloc(MAX_OBJECT_SIZE);
	recv(csock, buffer, MAX_OBJECT_SIZE, 0);
	printf("%s\n", buffer);
	
	http_request* request = string_to_http(buffer);
	if (request == NULL){
		printf("%s\n", "Invalid HTTP request");
		close(csock);
		return;
	}
	//free(buffer);

	proxy_strip_request(request);

	// Check cache
	char* response = read_cache(request->url);

	// Make request if cache miss
	if (response == NULL){
		//Make http request
		char* req_buffer = http_to_string(request);
		char* host = get_header(request, "Host");
		response = make_request(host, req_buffer);
		free(req_buffer);
		//Write it to cache
		write_cache(request->url, response);
	}

	//Send response
	Rio_writen(csock, response, MAX_OBJECT_SIZE);
	close(csock);
	//
	//free(response);
	//free_request(request);
	return;
}

void* thread(void* vargp){
	pthread_detach(pthread_self());
	while(1){
		int connfd = sbuf_remove(&sbuf);
		proxy_thread(connfd);
	}
}

int initialize_proxy(int port){
	// Create socket
	if ( (sock = socket(PF_INET, SOCK_STREAM, 0)) < 0){
		printf("%s\n", "Failed to socket.");
		return -1;
	}
	
	// Set sockaddr
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons((u_short) port);
	addr.sin_addr.s_addr = INADDR_ANY;

	// Bind socket
	if (bind(sock, (struct sockaddr *) &addr, sizeof(addr)) < 0){
		printf("%s\n", "Failed to bind.");
		return -1;
	}

	// Listen
	if (listen(sock, 5) < 0){
		printf("%s\n", "Failed to listen.");
		return -1;
	}

	// Success
	return 0;
}

int run_proxy(int port){
	// Setup proxy
	if (initialize_proxy(port) < 0){
		printf("%s%i%s\n", "in initialize_proxy(", port, ")");
		return -1;
	}

	// Create threads
	pthread_t tid;
	sbuf_init(&sbuf, SBUFSIZE);
	for (int i = 0; i < NTHREADS; i++){
		pthread_create(&tid, NULL, thread, NULL);
	}

	// Initialize cache
	init_cache(MAX_CACHE_SIZE, MAX_OBJECT_SIZE);

	// Accept incoming connections
	socklen_t client_size;
	struct sockaddr_in client_addr;
	client_size = sizeof(struct sockaddr_storage);
	while(1){
		int client_sock = accept(sock, (struct sockaddr *) &client_addr, &client_size);
		if (client_sock < 0){
			printf("%s\n", "Bad request");
			continue;
		}
		sbuf_insert(&sbuf, client_sock);
	}

	// Close proxy
	free_cache();
	close(sock);
	return 0;
}

int main(int argc, char** argv){
	
    if (argc < 2){
    	printf("%s\n", "Specify port");
    	exit(0);
    }
    int port = atoi(argv[1]);
    if (port == 0){
    	printf("%s\n", "Invalid port");
    	exit(0);
    }
    run_proxy(port);
    

    //char* sample = "GET http://localhost:1231/home.html HTTP/1.1\r\nHost: localhost:1231\r\nUser-Agent: curl/7.62.0\r\nAccept: */*\r\nProxy-Connection: Keep-Alive\r\n";
    /*
    http_request* req = string_to_http(sample);
    if (req == NULL){
    	printf("%s\n", "parse test failed (NULL)");
    	exit(0);
    }
    printf("%s", http_to_string(req));*/

    return 0;
}

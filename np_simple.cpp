#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <sys/socket.h>
#include <sys/types.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>

using namespace std;

void childHandler(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG)>0);
}

int main(int argc, char* argv[]){
	
	// socket bind listen accept
	signal(SIGCHLD,childHandler);

	int server_sockfd,client_sockfd;
	struct sockaddr_in server_addr,client_addr;
	
	int server_port = 5000;
	if(argc > 1){
		server_port = stoi(argv[1]);
	}

	if((server_sockfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP)) < 0){
		cout << "socket error" << endl;
		exit(EXIT_FAILURE);
	}

	int port_reuse = 1;	
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEADDR, &port_reuse, sizeof(port_reuse)) < 0){
		cout << "setsocketopt error" << endl;
		exit(EXIT_FAILURE);
	}
	// 
	if (setsockopt(server_sockfd, SOL_SOCKET, SO_REUSEPORT, &port_reuse, sizeof(port_reuse)) < 0){
		cout << "setsocketopt error" << endl;
		exit(EXIT_FAILURE);
	}

	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(server_port);
	if(bind(server_sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr)) < 0){
		cout << "bind error" << endl;
		exit(EXIT_FAILURE);
	}

	if(listen(server_sockfd,SOMAXCONN) < 0){
		cout << "listen error" << endl;
		exit(EXIT_FAILURE);
	}

	cout << "listening ...." << endl;
	
	socklen_t sin_size = sizeof(client_addr);
/*	char cwd[1024];
	if (getcwd(cwd, sizeof(cwd)) != NULL) {
		printf("Current working dir: %s\n", cwd);
	}
*/	while(1){
		if((client_sockfd = accept(server_sockfd,(struct sockaddr*)&client_addr,&sin_size)) < 0){
			cout << "accept error" << endl;
			return -1;
		}
		pid_t pid = fork();
		if(pid == 0){
			dup2(client_sockfd,STDIN_FILENO);
			dup2(client_sockfd,STDOUT_FILENO);
			dup2(client_sockfd,STDERR_FILENO);
			close(server_sockfd);
			close(client_sockfd);

			string cmd = "npshell";
			char* arg[2];
			arg[0] = &cmd[0];
			arg[1] = NULL;
			if((execv("/net/gcs/108/0856506/np2/npshell",arg)) < 0){
				cout << "exec error "<<errno<<endl;
				exit(EXIT_FAILURE);	
			}				
		}else{
			close(client_sockfd);
		}		
	}	
	return 0;
}

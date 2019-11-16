#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <cstring>
#include <string>
#include <vector>
#include <sys/select.h>
#include <sys/socket.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sstream>
#include <unistd.h>
#include <map>
#include <fcntl.h>

#define MAX_USER  30

using namespace std;

struct Pipe{
	int pipe_in;
	int pipe_out;
	int cmd_cntdwn;
};

struct CMD{
	vector<string> parsed_cmd;
	int N;
	int type;
	int user;
	string filename;
};

struct userInfo{
	int id;
	string nickname;
	string ip;
	string port;
};

struct per_user{
	map<string,vector<string>> env;
	vector<struct Pipe> pipe_table;
	int sockfd;
	struct userInfo user_info;	
};

struct system_info{
	struct per_user user_table[30];
	bool user_bitmap[30];
};

void print_error(string err){
	cout << err << endl;
	exit(EXIT_FAILURE);
}

void childHandler(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG)>0);
}

vector<struct CMD> parse_cmd(string cmd){
	vector<string> v_t;
	stringstream ss1(cmd);
	string str;
	while(ss1>>str){
		v_t.push_back(str);
		cout << str << endl;
	}

	vector<struct CMD> cmds;
	struct CMD cmd_t = {};
	bool cmd_remain;
	for(int i=0;i<v_t.size();i++){
		if(v_t[i][0]=='|'||v_t[i][0]=='!'){
			cmd_t.type = (v_t[i][0]=='|')?1:2;
			if(v_t[i].size()>1)
				cmd_t.N = stoi(v_t[i].substr(1,v_t[i].size()));
			else
				cmd_t.N = 1;
			cmds.push_back(cmd_t);
			cmd_t = {};
			cmd_remain = false;
		}else if(!strcmp(v_t[i].c_str(),">>")|| (v_t[i].size()==1 && v_t[i][0]=='>')){
			cmd_t.type = (!strcmp(v_t[i].c_str(),">>"))?4:3;
			i++;
			cmd_t.filename = v_t[i];
			cmds.push_back(cmd_t);
			cmd_t = {};
			cmd_remain = false;
		}else if(v_t[i][0]=='>' || v_t[i][0]=='<'){
			cout << "ho";
			cmd_t.type = (v_t[i][0]=='>')?5:6;
			cmd_t.user = stoi(v_t[i].substr(1,v_t[i].size()));
			cmds.push_back(cmd_t);
			cmd_t = {};
			cmd_remain = false;
		}else{
			cmd_t.parsed_cmd.push_back(v_t[i]);
			cmd_remain = true;
		}
	}
	cout << "s1 "<< cmds.size() << endl;
	if(cmd_remain) cmds.push_back(cmd_t);
	cout << "s2 " << cmds.size() << endl;
	return cmds;
}

int get_user_no(struct system_info& sys_info){
	for(int i=0;i<MAX_USER;i++){
		if(!sys_info.user_bitmap[i])
			return i;
	}
	return -1;
}

int search_user(struct system_info& sys_info,int client_fd){
	for(int i=0;i<MAX_USER;i++){
		if(sys_info.user_bitmap[i]){
			if(sys_info.user_table[i].sockfd == client_fd){
				return i;
			}
		}
	}
	return -1;	
}

void broadcast(struct system_info& sys_info, string content){
	for(int i=0;i<MAX_USER;i++){
		if(sys_info.user_bitmap[i]){
			write(sys_info.user_table[i].sockfd,&content[0],content.size());
		}
	}
}

void who(struct system_info& sys_info, int user_no){
	string result = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
	for(int i=0;i<MAX_USER;i++){
		if(sys_info.user_bitmap[i]){
			result += (to_string(i)+"\t"+sys_info.user_table[i].user_info.nickname+"\t"+sys_info.user_table[i].user_info.ip+":"+
					sys_info.user_table[i].user_info.port);
			if(i==user_no){
				result += "\t<-me";		
			}
			result += "\n";	
		}			
	}
	write(sys_info.user_table[user_no].sockfd,&result[0],result.size());
}

void tell(struct system_info& sys_info,int user_no,int recv_no,string content){
	string response = "*** "+sys_info.user_table[user_no].user_info.nickname+"told you ***: "+content+"\n";
	write(sys_info.user_table[recv_no].sockfd,&response[0],response.size());
}

void yell(struct system_info& sys_info,int user_no, string content){
	string response =  "*** "+sys_info.user_table[user_no].user_info.nickname+"yelled ***: "+content+"\n"; 
	broadcast(sys_info,response);
}

void name(struct system_info& sys_info, int user_no, string new_name){
	sys_info.user_table[user_no].user_info.nickname = new_name;
	string content = "*** User from "+sys_info.user_table[user_no].user_info.ip+":"+sys_info.user_table[user_no].user_info.port+" is named \'"+new_name+"\'. ***\n";
	broadcast(sys_info,content);	
}

void user_setenv(struct system_info& sys_info,int user_no,string key,string value){
	stringstream ss(value);
	string str;
	vector<string> v;
	while(getline(ss,str,':')){
		v.push_back(str);
	}
	sys_info.user_table[user_no].env[key] = v; 				
}

void user_printenv(struct system_info& sys_info,int user_no,string key){
	if(sys_info.user_table[user_no].env.find(key)!=sys_info.user_table[user_no].env.end()){
		vector<string> v = sys_info.user_table[user_no].env[key];
		string result = v[0];
		for(int i=1;i<v.size();i++){
			result += ":";
			result += v[i];
		}
		result += "\n";
		write(sys_info.user_table[user_no].sockfd,&result[0],result.size());
	}
}

pid_t create_process(string cmd_path, vector<string> arg, int cmd_std_in, int cmd_std_out, int type, vector<struct Pipe>& pipe_table){
	vector<char*> argv;
	for(int i=0;i<arg.size();i++){
		argv.push_back((char*)arg[i].c_str());
	}
	argv.push_back(NULL);

	pid_t pid;
	while((pid = fork())<0){
		usleep(1000);
	}

	if(pid == 0){
		dup2(cmd_std_in,STDIN_FILENO);
		dup2(cmd_std_out,STDOUT_FILENO);
		if(type==2){
			dup2(cmd_std_out,STDERR_FILENO);
		}

		if(type==3||type==4){
			close(cmd_std_out);
		}
		for(int i=0;i<pipe_table.size();i++){
			close(pipe_table[i].pipe_in);
			close(pipe_table[i].pipe_out);
		}

		if(execv(cmd_path.c_str(),&argv[0])<0){
			print_error("execv error");
		}
	}else{
		return pid;
	}

}

void update_pipe_table(vector<struct Pipe>& pipe_table){
	for(int i=0;i<pipe_table.size();i++){
		pipe_table[i].cmd_cntdwn--;
	}
}

int search_pipe(vector<struct Pipe>& pipe_table,int n){
	for(int i=0;i<pipe_table.size();i++){
		if(pipe_table[i].cmd_cntdwn==n){
			return i;	
		}
	}
	return -1;
}

void create_new_pipe(vector<struct Pipe>& pipe_table,int fd[],int N){
	pipe(fd);
	struct Pipe new_pipe;
	new_pipe.pipe_in = fd[1];
	new_pipe.pipe_out = fd[0];
	new_pipe.cmd_cntdwn = N;
	pipe_table.push_back(new_pipe);
}

void user_exit(struct system_info& sys_info,fd_set& active_fd_set,int user_no){
	int client_fd = sys_info.user_table[user_no].sockfd;	
	sys_info.user_bitmap[user_no] = false;
	cout << "close fd: "<< client_fd << endl;
	close(client_fd);
	FD_CLR(client_fd, &active_fd_set);
}

void set_new_user(struct system_info& sys_info,int client_fd,string ip,string port){
	int user_no = get_user_no(sys_info);
	user_setenv(sys_info,user_no,"PATH","bin:.");
	sys_info.user_table[user_no].sockfd =client_fd;
	sys_info.user_table[user_no].user_info.nickname = "(no name)";
	sys_info.user_table[user_no].user_info.ip = ip;
	sys_info.user_table[user_no].user_info.port = port;
	sys_info.user_bitmap[user_no] = true;	
	cout << user_no <<endl;
	string content = "*** User ’(no name)’ entered from"+ip+":"+port+". ***\n";
	broadcast(sys_info,content);
}

bool fileExist(string filename){
	struct stat buf;
	return (stat(filename.c_str(),&buf)==0);
}

void npshell(struct system_info& sys_info,fd_set& active_fd_set,int user_no){
	
	signal(SIGCHLD,childHandler);
	
	int client_fd = sys_info.user_table[user_no].sockfd;
	
	char buf[1024];
	memset(buf,0,sizeof(buf));
	read(client_fd,buf,sizeof(buf));

	stringstream ss(buf);
	string line;
	getline(ss,line,'\r');	
	
	vector<struct CMD> cmds = parse_cmd(line);
	pid_t last_cmd_pid;

	for(int i=0;i<cmds.size();i++){
			
		update_pipe_table(sys_info.user_table[user_no].pipe_table);
		if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"setenv")){
			user_setenv(sys_info,user_no,cmds[i].parsed_cmd[1],cmds[i].parsed_cmd[2]);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"printenv")){
			user_printenv(sys_info,user_no,cmds[i].parsed_cmd[1]);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"exit")){
			user_exit(sys_info,active_fd_set,user_no);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"who")){
			who(sys_info,user_no);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"name")){
			name(sys_info,user_no,cmds[i].parsed_cmd[1]);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"tell")){
			int recv_no = stoi(cmds[i].parsed_cmd[1]);
			tell(sys_info,user_no,recv_no,cmds[i].parsed_cmd[2]);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"yell")){
			yell(sys_info,user_no,cmds[i].parsed_cmd[1]);
			return;
		}
		
		string exe_path;
		bool file_existed = false;
		for(int j=0;j<sys_info.user_table[user_no].env["PATH"].size();j++){
			exe_path = sys_info.user_table[user_no].env["PATH"][j]+"/"+cmds[i].parsed_cmd[0];
			if(fileExist(exe_path)){
				file_existed = true;
				break;
			}
		}	
		if(!file_existed){
			cout << "Unknown command: [" << cmds[i].parsed_cmd[0] << "]." << endl;
			continue;
		}
	
		
		int client_fd = sys_info.user_table[user_no].sockfd;	
		int cmd_std_in,cmd_std_out;
		int p,p_fd[2],f_fd;

		p = search_pipe(sys_info.user_table[user_no].pipe_table,0);
		int pipe_of_this_cmd = p;
		if(p>=0){
			cmd_std_in = sys_info.user_table[user_no].pipe_table[p].pipe_out;
		}else{
			cmd_std_in = STDIN_FILENO;
		}

		if(cmds[i].type == 1 || cmds[i].type == 2){
			p = search_pipe(sys_info.user_table[user_no].pipe_table,cmds[i].N);
			if(p>=0){
				cmd_std_out = sys_info.user_table[user_no].pipe_table[p].pipe_in; 	
			}else{
				create_new_pipe(sys_info.user_table[user_no].pipe_table,p_fd,cmds[i].N);
				cmd_std_out = p_fd[1];
			}
		}else if(cmds[i].type == 3){
			f_fd = open(cmds[i].filename.c_str(),O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
			cmd_std_out = f_fd;
		}else{
			cmd_std_out = client_fd;
		}	
		last_cmd_pid = create_process(exe_path,cmds[i].parsed_cmd,cmd_std_in,cmd_std_out,cmds[i].type,sys_info.user_table[user_no].pipe_table);
		if(pipe_of_this_cmd >= 0){
			close(sys_info.user_table[user_no].pipe_table[pipe_of_this_cmd].pipe_in);
			close(sys_info.user_table[user_no].pipe_table[pipe_of_this_cmd].pipe_out);
			sys_info.user_table[user_no].pipe_table.erase(sys_info.user_table[user_no].pipe_table.begin()+pipe_of_this_cmd);
		}
		if(cmds[i].type==3){
			close(f_fd);
		}	
	}
	if(cmds.back().N <= 0){
		int status;
		waitpid(last_cmd_pid,&status,0);
	}
}

int main(int argc, char* argv[]){
	
	int server_sockfd;
	struct sockaddr_in server_addr; 

	int server_port = 5000;
	if(argc > 1){
		server_port = stoi(argv[1]);
	}

	if((server_sockfd = socket(AF_INET,SOCK_STREAM,0)) <  0){
		print_error("socket error");
	}
	cout << server_sockfd << endl;

	int port_reuse = 1;
	if(setsockopt(server_sockfd,SOL_SOCKET,SO_REUSEADDR,&port_reuse,sizeof(port_reuse))<0){
		print_error("setsocketopt error");
	}
		
	memset(&server_addr,0,sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(server_port);
	if(bind(server_sockfd,(struct sockaddr*)&server_addr,sizeof(server_addr))<0){
		print_error("bind error");
	}

	if(listen(server_sockfd,SOMAXCONN)<0){
		print_error("listen error");
	}

	string percent = "% ";
	struct system_info sys_info={};
	socklen_t len = sizeof(struct sockaddr_in);	
	int max_fd = server_sockfd;
	fd_set active_fd_set;
	FD_ZERO(&active_fd_set);
	FD_SET(server_sockfd, &active_fd_set);
	while(1){

		fd_set read_fds;
		
		struct timeval tv;
		tv.tv_sec = 2;
		tv.tv_usec = 0;
		
		read_fds = active_fd_set;
		int select_return = select(max_fd+1,&read_fds,NULL,NULL,&tv);
		
		//if select
		if(select_return == -1){ 
			//print_error("select error");
			cout << "select error" << endl;
			continue;
		}else if (select_return == 0){
			//cout << "select timeout" << endl;
		       	continue;	
		}else{
			for(int i =0;i<FD_SETSIZE;i++){
				if (FD_ISSET(i, &read_fds)) {
					if(i==server_sockfd){
						//new connection request
						struct sockaddr_in client_addr;
						int new_fd = accept(server_sockfd,(struct sockaddr*)&client_addr,&len);
						if(new_fd == -1){
							print_error("accept error");		
						}else{
							set_new_user(sys_info,new_fd,inet_ntoa(client_addr.sin_addr),to_string(ntohs(client_addr.sin_port)));					
							
							FD_SET(new_fd,&active_fd_set);
							if(new_fd > max_fd){
								max_fd = new_fd;
							}
							cout << "new_fd: "<< new_fd << endl;

							write(new_fd,&percent[0],percent.size()); 				
						}						
					}else{
						int client_fd = i;
						int user_no = search_user(sys_info,client_fd);
						npshell(sys_info,active_fd_set,user_no);
						write(client_fd,&percent[0],percent.size()); 
					}
				}
			}
		}				
	}
	return 0;
}

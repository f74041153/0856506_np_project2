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
	int in_type;
	int out_type;
	int N;
	int user_pipe_to;
	int user_pipe_from;
	string filename;
};

struct userInfo{
	int id;
	string nickname;
	string ip;
	string port;
};

struct userPipe{
	int pipe_in;
	int pipe_out;
};

struct per_user{
	map<string,vector<string>> env;
	vector<struct Pipe> pipe_table;
	int sockfd;
	struct userInfo user_info;
};

struct system_info{
	struct userPipe user_pipe[MAX_USER][MAX_USER];
	bool user_pipe_bitmap[MAX_USER][MAX_USER];
	struct per_user user_table[MAX_USER];
	bool user_bitmap[MAX_USER];
};

void create_user_pipe(struct system_info& sys_info,int pipe_from,int pipe_to){
	int fd[2];
	pipe(fd);
	sys_info.user_pipe[pipe_from][pipe_to].pipe_in = fd[1];
	sys_info.user_pipe[pipe_from][pipe_to].pipe_out = fd[0];
	sys_info.user_pipe_bitmap[pipe_from][pipe_to] = true;	
}

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
	vector<struct CMD> cmds;
	struct CMD cmd_t = {};
	stringstream ss1(cmd);
	string str;
	while(ss1>>str){
		if(!strcmp(str.c_str(),"yell")){
			string s1;
			getline(ss1,s1);
			cmd_t.parsed_cmd.push_back(str);
			cmd_t.parsed_cmd.push_back(s1);
			cout << str << " " << s1 << endl;
			cmds.push_back(cmd_t);
			return cmds;
		}else if(!strcmp(str.c_str(),"tell")){
			string s1,s2;
			ss1>>s1;
			getline(ss1,s2);
			cmd_t.parsed_cmd.push_back(str);
			cmd_t.parsed_cmd.push_back(s1);
			cmd_t.parsed_cmd.push_back(s2);
			cout << str << " " << s1 << " " << s2 << endl; 
			cmds.push_back(cmd_t);
			return cmds;			
		}
		v_t.push_back(str);
	}

	int in_out = 0;
	for(int i=0;i<v_t.size();i++){
		if(v_t[i][0]=='|'||v_t[i][0]=='!'){
			in_out++;
			cmd_t.out_type = (v_t[i][0]=='|')?1:2;
			if(v_t[i].size()>1)
				cmd_t.N = stoi(v_t[i].substr(1,v_t[i].size()));
			else
				cmd_t.N = 1;
		}else if(!strcmp(v_t[i].c_str(),">>")|| (v_t[i].size()==1 && v_t[i][0]=='>')){
			in_out++;
			cmd_t.out_type = (!strcmp(v_t[i].c_str(),">>"))?4:3;
			i++;
			cmd_t.filename = v_t[i];
		}else if(v_t[i][0]=='>'){
			in_out++;
			cmd_t.out_type = 5;
			cmd_t.user_pipe_to = stoi(v_t[i].substr(1,v_t[i].size()));
		}else if(v_t[i][0]=='<'){
			in_out++;
			cmd_t.in_type = 1;
			cmd_t.user_pipe_from = stoi(v_t[i].substr(1,v_t[i].size()));				
		}else{
			if(in_out>0){
				cmds.push_back(cmd_t);
			       	cmd_t = {};
				in_out=0;
			}
			cmd_t.parsed_cmd.push_back(v_t[i]);
		}
	}
	cmds.push_back(cmd_t);
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
			result += (to_string(i+1)+"\t"+sys_info.user_table[i].user_info.nickname+"\t"+sys_info.user_table[i].user_info.ip+":"+
					sys_info.user_table[i].user_info.port);
			if(i==user_no){
				result += "\t<-me";		
			}
			result += "\n";	
		}			
	}
	write(sys_info.user_table[user_no].sockfd,&result[0],result.size());
}

void tell(struct system_info& sys_info,int user_no, vector<string> parsed_cmd){
	
	int recv_no = stoi(parsed_cmd[1])-1;
	string response;
	if(!sys_info.user_bitmap[recv_no]){
		response = "*** Error: user #"+to_string(recv_no+1)+"does not exist yet. ***\n";
		write(sys_info.user_table[user_no].sockfd,&response[0],response.size());
		return;
	}

	string content = parsed_cmd[2];
	response = "*** "+sys_info.user_table[user_no].user_info.nickname+" told you ***: "+content+"\n";
	write(sys_info.user_table[recv_no].sockfd,&response[0],response.size());
}

void yell(struct system_info& sys_info,int user_no, string content){
	string response =  "*** "+sys_info.user_table[user_no].user_info.nickname+" yelled ***: "+content+"\n"; 
	broadcast(sys_info,response);
}

bool name_existed(struct system_info& sys_info, string new_name){
	for(int i=0;i<MAX_USER;i++){
		if(sys_info.user_bitmap[i]){
			if(!strcmp(sys_info.user_table[i].user_info.nickname.c_str(),new_name.c_str())){
				return true;
			}
		}
	}
	return false;
}

void name(struct system_info& sys_info, int user_no, string new_name){
	
	string rsp;
	if(name_existed(sys_info,new_name)){
		rsp = "*** User \'"+new_name+"\' already exists. ***\n";
		write(sys_info.user_table[user_no].sockfd,&rsp[0],rsp.size());
		return;		
	}

	sys_info.user_table[user_no].user_info.nickname = new_name;
	rsp = "*** User from "+sys_info.user_table[user_no].user_info.ip+":"+sys_info.user_table[user_no].user_info.port+" is named \'"+new_name+"\'. ***\n";
	broadcast(sys_info,rsp);	
}

void user_setenv(struct system_info& sys_info,int user_no,string key,string value){
	setenv(key.c_str(),value.c_str(),1);
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

pid_t create_process(string cmd_path, vector<string> arg, int cmd_std_in, int cmd_std_out, int cmd_std_err, int out_type, vector<struct Pipe>& pipe_table, struct system_info& sys_info){
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
		dup2(cmd_std_err,STDERR_FILENO);
		if(out_type==2){
			dup2(cmd_std_out,STDERR_FILENO);
		}
		/* close fd for write file */
		if(out_type==3||out_type==4){
			close(cmd_std_out);
		}
		/* close all user pipe copy */
		for(int i=0;i<MAX_USER;i++){
			for(int j=0;j<MAX_USER;j++){
				if(sys_info.user_pipe_bitmap[i][j]){
					close(sys_info.user_pipe[i][j].pipe_in);
					close(sys_info.user_pipe[i][j].pipe_out);
				}
			}
		}
		/* close all pipe copy */
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
	/* clean up all user pipe that pipe to this user */
	for(int i=0;i<MAX_USER;i++){
		if(sys_info.user_pipe_bitmap[i][user_no]){
			close(sys_info.user_pipe[i][user_no].pipe_in);
			close(sys_info.user_pipe[i][user_no].pipe_out);
			sys_info.user_pipe_bitmap[i][user_no]=false;
		}
	}
	close(client_fd);
	FD_CLR(client_fd, &active_fd_set);
	
	string rsp = "*** User \'"+sys_info.user_table[user_no].user_info.nickname+"\' left. ***\n";
	broadcast(sys_info,rsp);
}

void welcome_new_user(struct system_info& sys_info,int client_fd,string ip,string port){
	
	int user_no = get_user_no(sys_info);
	clearenv();
	user_setenv(sys_info,user_no,"PATH","bin:.");
	sys_info.user_table[user_no].sockfd =client_fd;
	sys_info.user_table[user_no].user_info.nickname = "(no name)";
	sys_info.user_table[user_no].user_info.ip = ip;
	sys_info.user_table[user_no].user_info.port = port;
	sys_info.user_bitmap[user_no] = true;	
	cout << user_no <<endl;
	
	string welcome_msg = "****************************************\n";
	welcome_msg += "** Welcome to the information server. **\n";
	welcome_msg += "****************************************\n";
	write(client_fd,&welcome_msg[0],welcome_msg.size());
	
	string content = "*** User '(no name)' entered from "+ip+":"+port+". ***\n";
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
	getline(ss,line);
	if(line.size()<1 || line[0]=='\r'){
		return;
	}
	
	vector<struct CMD> cmds = parse_cmd(line);
	
	pid_t last_cmd_pid;
	bool error = true;
	for(int i=0;i<cmds.size();i++){
		update_pipe_table(sys_info.user_table[user_no].pipe_table);
		/*built-in command*/
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
			tell(sys_info,user_no,cmds[i].parsed_cmd);
			return;
		}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"yell")){
			yell(sys_info,user_no,cmds[i].parsed_cmd[1]);
			return;
		}
		/* check whether command exists */
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
			string rsp = "Unknown command: [" + cmds[i].parsed_cmd[0] + "].\n" ;
			write(client_fd,&rsp[0],rsp.size());
			continue;
		}
	
		
		int cmd_std_in,cmd_std_out,cmd_std_err;
		int p,p_fd[2],f_fd;
		int pipe_of_this_cmd = -1;
		/* prepare cmd's stdin */
		if(cmds[i].in_type == 1) // clearly assign user pipe ex: cat <2
		{
			int pipe_from = cmds[i].user_pipe_from-1;
			if(!sys_info.user_bitmap[pipe_from]){
				string rsp = "*** Error: user #"+to_string(pipe_from+1)+" does not exist yet. ***\n";
				write(client_fd,&rsp[0],rsp.size()); 
				continue;
			}
			if(sys_info.user_pipe_bitmap[pipe_from][user_no] == false){
				string rsp = "*** Error: the pipe #"+to_string(pipe_from+1)+"->#"+to_string(user_no+1)+" does not exist yet. ***\n";
				write(client_fd,&rsp[0],rsp.size());
				continue;
			}
			cmd_std_in = sys_info.user_pipe[pipe_from][user_no].pipe_out;
		}else{
			p = search_pipe(sys_info.user_table[user_no].pipe_table,0);
			pipe_of_this_cmd = p;
			if(p>=0){
				cmd_std_in = sys_info.user_table[user_no].pipe_table[p].pipe_out;
			}else{
				cmd_std_in = STDIN_FILENO;
			}
		}

		/* prepare cmd's stdout*/
		if(cmds[i].out_type == 1 || cmds[i].out_type == 2){
			p = search_pipe(sys_info.user_table[user_no].pipe_table,cmds[i].N);
			if(p>=0){
				cmd_std_out = sys_info.user_table[user_no].pipe_table[p].pipe_in; 	
			}else{
				create_new_pipe(sys_info.user_table[user_no].pipe_table,p_fd,cmds[i].N);
				cmd_std_out = p_fd[1];
			}
		}else if(cmds[i].out_type == 3){
			f_fd = open(cmds[i].filename.c_str(),O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
			cmd_std_out = f_fd;
		}else if(cmds[i].out_type == 5){
			int pipe_to = cmds[i].user_pipe_to-1;
			if(!sys_info.user_bitmap[pipe_to]){ 
				string rsp = "*** Error: user #"+to_string(pipe_to+1)+" does not exist yet. ***\n" ;
				write(client_fd,&rsp[0],rsp.size());
				continue;
			}
			if(sys_info.user_pipe_bitmap[user_no][pipe_to] == true){
				string rsp = "*** Error: the pipe #"+to_string(user_no+1)+"->#"+to_string(pipe_to+1)+" already exists. ***\n";
				write(client_fd,&rsp[0],rsp.size());
				continue;	
			}
			create_user_pipe(sys_info,user_no,pipe_to);
			cmd_std_out = sys_info.user_pipe[user_no][pipe_to].pipe_in;
		}else{
			cmd_std_out = client_fd;
		}

		/*prepare cmd's stderr*/
		cmd_std_err = client_fd;

		/* create child process */
		last_cmd_pid = create_process(exe_path,cmds[i].parsed_cmd,cmd_std_in,cmd_std_out,cmd_std_err,cmds[i].out_type,sys_info.user_table[user_no].pipe_table,sys_info);
		error = false;
		
		/* close fd */
		if(pipe_of_this_cmd >= 0){
			close(sys_info.user_table[user_no].pipe_table[pipe_of_this_cmd].pipe_in);
			close(sys_info.user_table[user_no].pipe_table[pipe_of_this_cmd].pipe_out);
			sys_info.user_table[user_no].pipe_table.erase(sys_info.user_table[user_no].pipe_table.begin()+pipe_of_this_cmd);
		}
		if(cmds[i].out_type == 3){
			close(f_fd);
		}
		if(cmds[i].in_type == 1){
			int pipe_from = cmds[i].user_pipe_from-1;
			string rsp = "*** "+sys_info.user_table[user_no].user_info.nickname;
			rsp += " (#"+to_string(user_no+1)+") just received from "+sys_info.user_table[pipe_from].user_info.nickname;
			rsp += " (#"+to_string(pipe_from+1)+") by \'"+line+"\' ***\n";	
			broadcast(sys_info,rsp);
			
			close(sys_info.user_pipe[pipe_from][user_no].pipe_in);
			close(sys_info.user_pipe[pipe_from][user_no].pipe_out);
			sys_info.user_pipe_bitmap[pipe_from][user_no] = false;
			
		}
		if(cmds[i].out_type ==5){
			int pipe_to = cmds[i].user_pipe_to-1;
			string rsp = "*** "+sys_info.user_table[user_no].user_info.nickname;
			rsp += " (#"+to_string(user_no+1)+") just piped \'"+line+"\' to "+sys_info.user_table[pipe_to].user_info.nickname;
			rsp += " (#"+to_string(pipe_to+1)+") ***\n";									
			broadcast(sys_info,rsp);
		}	
	}
	/* the line have things to output to screen */
	if(!error && cmds.back().out_type == 0){
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
			cout << "select error" << endl;
			continue;
		}else if (select_return == 0){
		       	continue;	
		}else{
			for(int i =0;i<FD_SETSIZE;i++){
				if (FD_ISSET(i, &read_fds)) {
					if(i==server_sockfd){
						//new connection request
						struct sockaddr_in client_addr;
						int new_fd = accept(server_sockfd,(struct sockaddr*)&client_addr,&len);
						if(new_fd == -1){
							cout << errno << endl;
							print_error("accept error");		
						}else{
							welcome_new_user(sys_info,new_fd,inet_ntoa(client_addr.sin_addr),to_string(ntohs(client_addr.sin_port)));					
							
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

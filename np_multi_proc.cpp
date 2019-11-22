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
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h> 
#include <sys/sem.h>

#define MAX_USER  30

using namespace std;

union semun{
	int val;
	struct semid_ds* buf;
	unsigned short* array;
};

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
	char nickname[256];
	char ip[256];
	char port[256];
};

struct userPipe{
	bool signaled;
	int writefd;
	int readfd;
};

struct MSG{
	char msg_queue[256][1024];
	int read_ptr;
	int write_ptr;
};

struct per_user{
	struct userInfo user_info;
	struct MSG msgbox;
	int pid;
};

struct system_info{
	struct userPipe user_pipe[MAX_USER][MAX_USER];
	int  user_pipe_bitmap[MAX_USER][MAX_USER];
	struct per_user user_table[MAX_USER];
	bool user_bitmap[MAX_USER];
};

struct system_info* sys_info;
int sys_info_shmid,semid;
int whoami;

int sem_create(int key,int sem_val){
	int sem_id;
	union semun sem_union;
	sem_id = semget(key,1,0666|IPC_CREAT);
	if(sem_val >=0){
		sem_union.val = sem_val;
		if(semctl(sem_id,0,SETVAL,sem_union)==-1){
			return -1;
		}
	}
	return sem_id;
}

int sem_rm(int sem_id){
	union semun sem_union;
	if(semctl(sem_id,0,IPC_RMID,sem_union)==-1){
		return -1;
	}else{
		return 0;
	}
}

int sem_wait(int sem_id){
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = -1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(sem_id,&sem_b,1)==-1){
		fprintf(stderr,"semaphore_p failed\n");
		return -1;
	}
	return 0;
}

int sem_signal(int sem_id){
	struct sembuf sem_b;
	sem_b.sem_num = 0;
	sem_b.sem_op = 1;
	sem_b.sem_flg = SEM_UNDO;
	if(semop(sem_id,&sem_b,1)==-1){
		fprintf(stderr,"semaphore_v failed\n");
		return -1;
	}
	return 0;
}
/* signal handler for ctrl-c */
void system_rm_shm(int signo){
	cout << "server finish" << endl;
	shmdt(sys_info);
	shmctl(sys_info_shmid,IPC_RMID,(struct shmid_ds*)0);
	sem_rm(semid);
	exit(0);
}

/* signal handler for sigusr1 */
void read_msgbox(int signo){
	int& ptr = sys_info->user_table[whoami].msgbox.read_ptr;
	ptr++;
	ptr=ptr%256;
	write(1,sys_info->user_table[whoami].msgbox.msg_queue[ptr],strlen(sys_info->user_table[whoami].msgbox.msg_queue[ptr]));
	signal(signo,read_msgbox);
}

/* signal handler for sigusr2*/
void create_user_pipe(int signo){
	for(int i=0;i<MAX_USER;i++){
		if(sys_info->user_pipe[i][whoami].signaled){
			int from = i;
			sys_info->user_pipe[from][whoami].signaled = false;
			string user_pipe_file = "user_pipe/"+to_string(from)+"_"+to_string(whoami);
			int readfd = open(user_pipe_file.c_str(),O_RDONLY,0666);
			sys_info->user_pipe[from][whoami].readfd = readfd;
		      	return;	
		}
	}
} 

void write_msgbox(int userno,string content){
	int& ptr = sys_info->user_table[userno].msgbox.write_ptr;
	strcpy(sys_info->user_table[userno].msgbox.msg_queue[ptr],content.c_str());
	ptr++;
	ptr=ptr%256;
	pid_t pid = sys_info->user_table[userno].pid;
	kill(pid,SIGUSR1);

}

void request_user_pipe(int pipe_from,int pipe_to){
//	sem_wait(semid);
	sys_info->user_pipe[pipe_from][pipe_to].signaled = true;
	pid_t pid = sys_info->user_table[pipe_to].pid;
	string user_pipe_file = "user_pipe/"+to_string(pipe_from)+"_"+to_string(pipe_to);
	int mk = mknod(user_pipe_file.c_str(),S_IFIFO|0666,0);
//	sem_signal(semid);

	kill(pid,SIGUSR2);
	
//	sem_wait(semid);
	int writefd = open(user_pipe_file.c_str(),O_WRONLY,0666);
	sys_info->user_pipe[pipe_from][pipe_to].writefd = writefd;
	sys_info->user_pipe_bitmap[pipe_from][pipe_to] = true;
	remove(user_pipe_file.c_str());
//	sem_signal(semid);
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
			cmds.push_back(cmd_t);
			return cmds;
		}else if(!strcmp(str.c_str(),"tell")){
			string s1,s2;
			ss1>>s1;
			getline(ss1,s2);
			cmd_t.parsed_cmd.push_back(str);
			cmd_t.parsed_cmd.push_back(s1);
			cmd_t.parsed_cmd.push_back(s2);
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

int get_user_no(){
	for(int i=0;i<MAX_USER;i++){
		if(!sys_info->user_bitmap[i])
			return i;
	}
	return -1;
}

void broadcast(string content){
	for(int i=0;i<MAX_USER;i++){
		bool user_exist = sys_info->user_bitmap[i];
		if(user_exist){
			write_msgbox(i,content);
		}
	}
}

void who(){
//	sem_wait(semid);
	string result = "<ID>\t<nickname>\t<IP:port>\t<indicate me>\n";
	for(int i=0;i<MAX_USER;i++){
		if(sys_info->user_bitmap[i]){
			result += (to_string(i+1)+"\t"+sys_info->user_table[i].user_info.nickname+"\t"+sys_info->user_table[i].user_info.ip+":"+
					sys_info->user_table[i].user_info.port);
			if(i==whoami){
				result += "\t<-me";		
			}
			result += "\n";	
		}			
	}
	cout << result;
//	sem_signal(semid);
}

void tell(vector<string> parsed_cmd){
//	sem_wait(semid);
	int recv_no = stoi(parsed_cmd[1])-1;
	string response;
	if(!sys_info->user_bitmap[recv_no]){
		response = "*** Error: user #"+to_string(recv_no+1)+"does not exist yet. ***\n";
		cout << response;
//		sem_signal(semid);
		return;
	}
	string content = parsed_cmd[2];
	response = "*** "+string(sys_info->user_table[whoami].user_info.nickname)+" told you ***: "+content+"\n";
	write_msgbox(recv_no,response);
//	sem_signal(semid);
}

void yell(string content){
//	sem_wait(semid);
	string response =  "*** "+string(sys_info->user_table[whoami].user_info.nickname)+" yelled ***: "+content+"\n"; 
	broadcast(response);
//	sem_signal(semid);
}

bool name_existed(string new_name){
	for(int i=0;i<MAX_USER;i++){
		if(sys_info->user_bitmap[i]){
			if(!strcmp(sys_info->user_table[i].user_info.nickname,new_name.c_str())){
				return true;
			}
		}
	}
	return false;
}

void name(string new_name){
	string rsp;
//	sem_wait(semid);
	if(name_existed(new_name)){
		rsp = "*** User \'"+new_name+"\' already exists. ***\n";
		cout << rsp;
//		sem_signal(semid);
		return;		
	}
	strcpy(sys_info->user_table[whoami].user_info.nickname,new_name.c_str());
	rsp = "*** User from "+string(sys_info->user_table[whoami].user_info.ip)+":"+string(sys_info->user_table[whoami].user_info.port)+" is named \'"+new_name+"\'. ***\n";
	broadcast(rsp);
//	sem_signal(semid);
}

void user_setenv(string key,string value,map<string,vector<string>>& env){
	setenv(key.c_str(),value.c_str(),1);
	stringstream ss(value);
	string str;
	vector<string> v;
	while(getline(ss,str,':')){
		v.push_back(str);
	}
	env[key] = v;				
}

void user_printenv(string key,map<string,vector<string>>& env){
	if(env.find(key)!=env.end()){
		vector<string> v = env[key];
		string result = v[0];
		for(int i=1;i<v.size();i++){
			result += ":";
			result += v[i];
		}
		cout << result << endl;
	}
}

pid_t create_process(string cmd_path, vector<string> arg, int cmd_std_in, int cmd_std_out, int cmd_std_err,int in_type,int out_type, vector<struct Pipe>& pipe_table){
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
	//	dup2(cmd_std_err,STDERR_FILENO);
		if(out_type==2){
			dup2(cmd_std_out,STDERR_FILENO);
		}
		/* 3,4 : close fd for write file, 5: close fd for write fifo */
		if(out_type==3||out_type==4||out_type==5){
			close(cmd_std_out);
		}

		/* close fd for read fifo */
		if(in_type==1){
			close(cmd_std_in);
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

void user_exit(){
//	sem_wait(semid);
	string rsp = "*** User \'"+string(sys_info->user_table[whoami].user_info.nickname)+"\' left. ***\n";
	broadcast(rsp);
	sys_info->user_bitmap[whoami] = false;
	/* clean up all user pipe that pipe to this user */
	for(int i=0;i<MAX_USER;i++){
		if(sys_info->user_pipe_bitmap[i][whoami]){
			close(sys_info->user_pipe[i][whoami].readfd);
			close(sys_info->user_pipe[i][whoami].writefd);
			sys_info->user_pipe_bitmap[i][whoami] = false;
		}
	}
//	string rsp = "*** User \'"+string(sys_info->user_table[whoami].user_info.nickname)+"\' left. ***\n";
//	broadcast(rsp);
//	sem_signal(semid);
	exit(EXIT_SUCCESS);
}

void welcome_new_user(string ip,string port){
	
//	sem_wait(semid);
	whoami = get_user_no();
	strcpy(sys_info->user_table[whoami].user_info.nickname,"(no name)");
	strcpy(sys_info->user_table[whoami].user_info.ip,ip.c_str());
	strcpy(sys_info->user_table[whoami].user_info.port,port.c_str());
	memset(sys_info->user_table[whoami].msgbox.msg_queue,0,sizeof(sys_info->user_table[whoami].msgbox.msg_queue));
	sys_info->user_table[whoami].msgbox.write_ptr = 0;
	sys_info->user_table[whoami].msgbox.read_ptr = -1;

	sys_info->user_table[whoami].pid = getpid();
	sys_info->user_bitmap[whoami] = true;	
	
	string welcome_msg = "****************************************\n";
	welcome_msg += "** Welcome to the information server. **\n";
	welcome_msg += "****************************************\n";
	cout << welcome_msg;
	
	string content = "*** User '(no name)' entered from "+ip+":"+port+". ***\n";
	broadcast(content);
//	sem_signal(semid);
}

bool fileExist(string filename){
	struct stat buf;
	return (stat(filename.c_str(),&buf)==0);
}

void npshell(string ip,string port){
	
	map<string,vector<string>> env;
	vector<struct Pipe> pipe_table;

	signal(SIGCHLD,childHandler);
	signal(SIGUSR1,read_msgbox);
	signal(SIGUSR2,create_user_pipe);

	clearenv();	
	user_setenv("PATH","bin:.",env);
	welcome_new_user(ip,port);	
	
	string line;
	cout << "% ";			
	while(getline(cin,line)){

		if(line.size()<1 || line[0]=='\r'){
			cout << "% ";
			continue;
		}

		vector<struct CMD> cmds = parse_cmd(line);
		pid_t last_cmd_pid;
		bool error = true;
		for(int i=0;i<cmds.size();i++){
			update_pipe_table(pipe_table);
			/* built-in command*/
			if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"setenv")){
				user_setenv(cmds[i].parsed_cmd[1],cmds[i].parsed_cmd[2],env);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"printenv")){
				user_printenv(cmds[i].parsed_cmd[1],env);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"exit")){
				user_exit();
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"who")){
				who();
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"name")){
				name(cmds[i].parsed_cmd[1]);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"tell")){
				tell(cmds[i].parsed_cmd);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"yell")){
				yell(cmds[i].parsed_cmd[1]);
				continue;
			}
			/* check whether command exist */
			string exe_path;
			bool file_existed = false;
			for(int j=0;j<env["PATH"].size();j++){
				exe_path = env["PATH"][j]+"/"+cmds[i].parsed_cmd[0];
				if(fileExist(exe_path)){
					file_existed = true;
					break;
				}
			}	
			if(!file_existed){
				string rsp = "Unknown command: [" + cmds[i].parsed_cmd[0] + "].\n" ;
				cout << rsp;
				continue;
			}
		
			
			int cmd_std_in,cmd_std_out,cmd_std_err;
			int p,p_fd[2],f_fd;
			int pipe_of_this_cmd = -1;
			
			/* prepare cmd's stdin */
			if(cmds[i].in_type == 1){ // clearly assign user pipe ex: cat <2
//				sem_wait(semid);
				int pipe_from = cmds[i].user_pipe_from-1;
				if(!sys_info->user_bitmap[pipe_from]){
					string rsp = "*** Error: user #"+to_string(pipe_from+1)+" does not exist yet. ***\n";
					cout << rsp;
//					sem_signal(semid);
					continue;
				}
				if(sys_info->user_pipe_bitmap[pipe_from][whoami] == false){
					string rsp = "*** Error: the pipe #"+to_string(pipe_from+1)+"->#"+to_string(whoami+1)+" does not exist yet. ***\n";
					cout << rsp ;
//					sem_signal(semid);
					continue;
				}
				cmd_std_in = sys_info->user_pipe[pipe_from][whoami].readfd;
				string rsp = "*** "+string(sys_info->user_table[whoami].user_info.nickname);
				rsp += " (#"+to_string(whoami+1)+") just received from "+string(sys_info->user_table[pipe_from].user_info.nickname);
				rsp += " (#"+to_string(pipe_from+1)+") by \'"+line+"\' ***\n";  
				broadcast(rsp);
//				sem_signal(semid);
			}else{
				p = search_pipe(pipe_table,0);
				pipe_of_this_cmd = p;
				if(p>=0){
					cmd_std_in = pipe_table[p].pipe_out;
				}else{
					cmd_std_in = STDIN_FILENO;
				}
			}

			/* prepare cmd's stdout*/
			if(cmds[i].out_type == 1 || cmds[i].out_type == 2){
				p = search_pipe(pipe_table,cmds[i].N);
				if(p>=0){
					cmd_std_out = pipe_table[p].pipe_in; 	
				}else{
					create_new_pipe(pipe_table,p_fd,cmds[i].N);
					cmd_std_out = p_fd[1];
				}
			}else if(cmds[i].out_type == 3){
				f_fd = open(cmds[i].filename.c_str(),O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
				cmd_std_out = f_fd;
			}else if(cmds[i].out_type == 5){ //"cat a.txt >2"
//				sem_wait(semid);
				int pipe_to = cmds[i].user_pipe_to-1;
				if(!sys_info->user_bitmap[pipe_to]){ 
					string rsp = "*** Error: user #"+to_string(pipe_to+1)+" does not exist yet. ***\n" ;
					cout << rsp;
//					sem_signal(semid);
					continue;
				}
				if(sys_info->user_pipe_bitmap[whoami][pipe_to] == true){
					string rsp = "*** Error: the pipe #"+to_string(whoami+1)+"->#"+to_string(pipe_to+1)+" already exists. ***\n";
					cout << rsp;
//					sem_signal(semid);
					continue;	
				}
//				sem_signal(semid);
				request_user_pipe(whoami,pipe_to);
//				sem_wait(semid);
				cmd_std_out = sys_info->user_pipe[whoami][pipe_to].writefd;
				string rsp = "*** "+string(sys_info->user_table[whoami].user_info.nickname);
				rsp += " (#"+to_string(whoami+1)+") just piped '"+line+"' to "+string(sys_info->user_table[pipe_to].user_info.nickname);
				rsp += " (#"+to_string(pipe_to+1)+") ***\n";                            
				broadcast(rsp);
//				sem_signal(semid);
			}else{
				cmd_std_out = STDOUT_FILENO;
			}


			/* create child process */
			last_cmd_pid = create_process(exe_path,cmds[i].parsed_cmd,cmd_std_in,cmd_std_out,cmd_std_err,cmds[i].in_type,cmds[i].out_type,pipe_table);	
			error = false;
			
			/* close fd*/
			if(pipe_of_this_cmd >= 0){
				close(pipe_table[pipe_of_this_cmd].pipe_in);
				close(pipe_table[pipe_of_this_cmd].pipe_out);
				pipe_table.erase(pipe_table.begin()+pipe_of_this_cmd);
			}

			if(cmds[i].out_type == 3){
				close(f_fd);
			}
			if(cmds[i].in_type == 1){
//				sem_wait(semid);
				int pipe_from = cmds[i].user_pipe_from-1;				
				close(sys_info->user_pipe[pipe_from][whoami].readfd);
				sys_info->user_pipe_bitmap[pipe_from][whoami] = false;
//				sem_signal(semid);
			}
			if(cmds[i].out_type ==5){
//				sem_wait(semid);
				int pipe_to = cmds[i].user_pipe_to-1;
				close(sys_info->user_pipe[whoami][pipe_to].writefd);
//				sem_signal(semid);
			}	
		}
		if(!error && cmds.back().out_type == 0){
			int status;
			waitpid(last_cmd_pid,&status,0);
		}
		cout << "% ";
	}
	exit(EXIT_SUCCESS);
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

	/*ctrl-c, let master detach and remove share mem*/
	signal(SIGINT,system_rm_shm);

	/* share memory & semaphore */
	int shm_key = 3654,sem_key = 7777,sem_key2 = 8888;
	sys_info_shmid = shmget(shm_key,sizeof(struct system_info)*30,IPC_CREAT|0666);
	sys_info = (struct system_info*)shmat(sys_info_shmid,NULL,0);
	semid = sem_create(sem_key,1);


	socklen_t len = sizeof(struct sockaddr_in);	
	int client_fd;
	while(1){
		struct sockaddr_in client_addr;
		if((client_fd = accept(server_sockfd,(struct sockaddr*)&client_addr,&len))<0){
			cout << errno << endl;
			print_error("accept error");
		}
		pid_t pid = fork();
		if(pid ==0){
			dup2(client_fd,STDIN_FILENO);
			dup2(client_fd,STDOUT_FILENO);
			dup2(client_fd,STDERR_FILENO);
			close(server_sockfd);
			close(client_fd);
			npshell(inet_ntoa(client_addr.sin_addr),to_string(ntohs(client_addr.sin_port)));
		}else{
			close(client_fd);
		}								
	}
	return 0;
}

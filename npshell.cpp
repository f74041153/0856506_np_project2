# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <iostream>
# include <vector>
# include <map>
# include <string>
# include <sstream>
# include <sys/stat.h>
# include <sys/wait.h>
# include <sys/types.h>
# include <sys/stat.h>
# include <fcntl.h>
# include <unistd.h> 

using namespace std;

map<string,vector<string>> env;

struct Pipe{
	int pipe_in;
	int pipe_out;
	int cmd_cntdwn;
};

struct CMD{
	vector<string> parsed_cmd;
	int N;
	int type;
	string filename;
};

void my_setenv(string name,string value){
	stringstream ss(value);
	string str;
	vector<string> v;
	while(getline(ss,str,':')){
		v.push_back(str);
	}
	env[name] = v;
}

void my_printenv(string name){
	if(env.find(name)!=env.end()){
		vector<string> v = env[name];
		cout << v[0];
		for(int i=1;i<v.size();i++){
			cout << ":";
			cout << v[i]; 
		}
		cout << endl;
	}	
}

void my_exit(){
	exit(0);
}

void childHandler(int signo){
	int status;
	while(waitpid(-1,&status,WNOHANG)>0);
}

bool isNumber(string str){
	for(int i =0;i<str.size();i++){
		if(!isdigit(str[i]))
			return false;
	}
	return true;	
}

vector<struct CMD> parse_cmd (string cmd){
	// parse by space
	vector<string> v_t;
	stringstream ss1(cmd);
	string str;
	while(ss1 >> str){
		v_t.push_back(str);
	}

	vector<struct CMD> cmds;
	struct CMD cmd_t = {};
	bool cmd_remain;
	for(int i=0;i<v_t.size();i++){
		if(v_t[i][0] == '|' || v_t[i][0] == '!'){
			cmd_t.type = (v_t[i][0]=='|')?1:2;
			if(v_t[i].size()>1)
				cmd_t.N = stoi(v_t[i].substr(1,v_t[i].size()));
			else 
				cmd_t.N = 1;
			cmds.push_back(cmd_t);
			cmd_t = {};
			cmd_remain = false;	
		}else if(v_t[i][0]=='>'){
			cmd_t.type = 3;
			i++;
			cmd_t.filename = v_t[i];
			cmds.push_back(cmd_t);
			cmd_t = {};
			cmd_remain = false;
		}else{
			cmd_t.parsed_cmd.push_back(v_t[i]);
			cmd_remain = true;
		}
	}
	if(cmd_remain) cmds.push_back(cmd_t);
	return cmds;
}

bool fileExist(string filename){
	struct stat buf;
	return (stat(filename.c_str(),&buf) == 0);
}

pid_t create_process(string cmd_path, vector<string> arg, int cmd_std_in, int cmd_std_out, int type, vector<struct Pipe>& pipe_table){
	// rearrange execv arg
	vector<char*>argv;
	for(int i=0;i<arg.size();i++){
		argv.push_back((char*)arg[i].c_str());
	}
	argv.push_back(NULL);	
	
	// fork process
	pid_t pid;
       	while((pid = fork())<0){
		usleep(1000);
	}

	if(pid < 0){
		cout << "fail" << endl;
		exit(EXIT_FAILURE);
	}else if (pid == 0){ // child		
		dup2(cmd_std_in,STDIN_FILENO);
		dup2(cmd_std_out,STDOUT_FILENO);
		if(type == 2){
			dup2(cmd_std_out,STDERR_FILENO);
		}

		// dup done, close all pipe and file description
		if(type == 3){
			close(cmd_std_out);		
		}
		for(int i=0;i<pipe_table.size();i++){
			close(pipe_table[i].pipe_in);
			close(pipe_table[i].pipe_out);
		}
			
		if(execv(cmd_path.c_str(), &argv[0]) < 0){
			exit(EXIT_FAILURE);
		}
	}else{ // parent
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

int main(){
	/* default path */
	my_setenv("PATH","bin:.");	
	vector<struct Pipe> pipe_table;
	string line;
	cout << "% ";
	while(getline(cin,line)){
		/* handle input */
		vector<struct CMD> cmds = parse_cmd(line);	
		
		pid_t last_cmd_pid;
		signal(SIGCHLD,childHandler);
		for(int i=0;i<cmds.size();i++){
			/* update pipe table with change in command */
			update_pipe_table(pipe_table);

			/* filt out built-in command */
			if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"setenv")){
				my_setenv(cmds[i].parsed_cmd[1],cmds[i].parsed_cmd[2]);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"printenv")){
				my_printenv(cmds[i].parsed_cmd[1]);
				continue;
			}else if(!strcmp(cmds[i].parsed_cmd[0].c_str(),"exit")){
				my_exit();
				continue;
			}
	
			/* find out exe in env */		
			string exe_path; 
			bool file_existed = false;
			for(int j=0;j<env["PATH"].size();j++){
				exe_path = env["PATH"][j]+"/"+ cmds[i].parsed_cmd[0];
				if(fileExist(exe_path)){
					file_existed = true;
					break;
				}
			}
			if(!file_existed){
				 cout << "Unknown command: [" << cmds[i].parsed_cmd[0] << "]." <<endl;
				 continue;
			}
			
			/* below are executable cmd */
			int cmd_std_in, cmd_std_out;
			int p, p_fd[2], f_fd;	
			/* prepare stdin for this cmd */
			p = search_pipe(pipe_table,0);
			int pipe_of_this_cmd = p;
			if(p >= 0){
				cmd_std_in = pipe_table[p].pipe_out;
			}else {
				cmd_std_in = STDIN_FILENO;
			}
			/* prepare stdout for this cmd */
			if(cmds[i].type == 1 || cmds[i].type ==2){ // '|'or'!'
				p = search_pipe(pipe_table,cmds[i].N);
				if(p < 0){
					create_new_pipe(pipe_table,p_fd,cmds[i].N);
					cmd_std_out = p_fd[1]; 
				}else{
					cmd_std_out = pipe_table[p].pipe_in;		
				}				
			}else if(cmds[i].type == 3){ // '>'
				f_fd = open(cmds[i].filename.c_str(),O_RDWR|O_CREAT|O_TRUNC,S_IRWXU|S_IRWXG|S_IRWXO);
				cmd_std_out = f_fd;
			}else{
				cmd_std_out = STDOUT_FILENO;
			}
			/* create child process to execute cmd*/
			last_cmd_pid = create_process(exe_path,cmds[i].parsed_cmd,cmd_std_in,cmd_std_out,cmds[i].type,pipe_table);
			/* parent do this*/
			if(pipe_of_this_cmd >= 0) {
				close(pipe_table[pipe_of_this_cmd].pipe_in);
				close(pipe_table[pipe_of_this_cmd].pipe_out);
				pipe_table.erase(pipe_table.begin()+pipe_of_this_cmd);
			}
			if(cmds[i].type == 3){
				close(f_fd);
			}			
		}
		/* wait last cmd to be done (this line has destination to file or screen) */
		if(cmds.back().N <= 0){
			int status;
			waitpid(last_cmd_pid,&status,0);
		}
		cout << "% ";
	}	
	return 0;
}

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
# include <unistd.h>

using namespace std;

map<string,string> env;

struct Pipe{
	int pipe_in;
	int pipe_out;
	int cmd_cntdwn;
};

struct CMD{
	vector<string> parsed_cmd;
	int N=0;
};

void my_setenv(string name,string value){
	env[name]=value;
}

void my_printenv(string name){
	if(env.find(name)!=env.end())
		cout << env[name] << endl;	
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

	vector<struct CMD> parsed_cmd;
	stringstream ss1(cmd);
	string str;
	while(getline(ss1,str,'|')){		
		stringstream ss2(str);
		struct CMD tmp_cmd;
		bool check_pipeN = true,notEndwithN=false;
		while(ss2 >> str){
			if(check_pipeN && isNumber(str)){
				check_pipeN = false;
				parsed_cmd.back().N = stoi(str);
			}else{
				notEndwithN=true;
				tmp_cmd.parsed_cmd.push_back(str);
			}
		}
		if(notEndwithN)parsed_cmd.push_back(tmp_cmd);

	}
	return parsed_cmd;
}

bool fileExist(string filename){
	struct stat buf;
	return (stat(filename.c_str(),&buf) == 0);
}

pid_t create_process(string cmd_path,vector<string> arg,int pipe_in,int pipe_out){
	// process execv arg
	vector<char*>argv;
	for(int i=0;i<arg.size();i++){
		argv.push_back((char*)arg[i].c_str());
	}
	argv.push_back(NULL);
	
	// fork process
	pid_t pid = fork();
	if(pid < 0){
		exit(EXIT_FAILURE);
	}else if (pid == 0){
		// child
		if(pipe_in != STDIN_FILENO){
			dup2(pipe_in,STDIN_FILENO);
			close(pipe_in);
		}
		if (pipe_out != STDOUT_FILENO){
			dup2(pipe_out,STDOUT_FILENO);
			close(pipe_out);
		}	
				
		if(execv(cmd_path.c_str(), &argv[0]) < 0){
			exit(EXIT_FAILURE);
		}
	}else{
		// parent
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
	new_pipe.pipe_in = fd[0];
	new_pipe.pipe_out = fd[1];
	new_pipe.cmd_cntdwn = N;
	pipe_table.push_back(new_pipe);
}

int main(){

	/* default path */
	my_setenv("PATH","bin:.");

	vector<struct Pipe> pipe_table;
	cout << "% ";
	while(1){
		
		string line;
		getline(cin,line);

		/* handle input*/
		vector<struct CMD> cmds = parse_cmd(line);
		
		/* check whether is built-in command */
		if(!strcmp(cmds[0].parsed_cmd[0].c_str(),"setenv")){
			my_setenv(cmds[0].parsed_cmd[1],cmds[0].parsed_cmd[2]);
		}else if(!strcmp(cmds[0].parsed_cmd[0].c_str(),"printenv")){
			my_printenv(cmds[0].parsed_cmd[1]);
		}else if(!strcmp(cmds[0].parsed_cmd[0].c_str(),"exit")){
			my_exit();
		}
		
		string exe_path = "bin/" + cmds[0].parsed_cmd[0];
		if(fileExist(exe_path)){
			
			pid_t last_cmd_pid;
			signal(SIGCHLD,childHandler);
			
			for(int i=0;i<cmds.size();i++){
				
				int cmd_std_in,cmd_std_out,p,fd[2];
				update_pipe_table(pipe_table);
				
				// stdin for this cmd
				p = search_pipe(pipe_table,0);
				if(p>=0) cmd_std_in = pipe_table[p].pipe_out;
				else cmd_std_in = STDIN_FILENO;

				// stdout for this cmd 
				if(cmds[i].N>0){
					// pipeN
					p = search_pipe(pipe_table,cmds[i].N);
					if(p<0){
						// create new pipe
						create_new_pipe(pipe_table,fd,cmds[i].N);
						cmd_std_out = fd[0];
					}else{
						cmd_std_out = pipe_table[p].pipe_in;		
					}				
				}else if(i<cmds.size()-1){
					// pipe
				//	cout << 1 << endl;
					p = search_pipe(pipe_table,1);
					if(p<0){
						create_new_pipe(pipe_table,fd,1);
						cmd_std_out = fd[0];
					}else{
						cmd_std_out = pipe_table[p].pipe_in;	
					}
				}else{
				//	cout << 0 << endl;
					cmd_std_out = STDOUT_FILENO; 
				}
				cout << STDIN_FILENO << "\t" << cmd_std_in << endl;
				cout << STDOUT_FILENO << "\t"<<cmd_std_out << endl;
				
				last_cmd_pid = create_process(exe_path,cmds[0].parsed_cmd,cmd_std_in,cmd_std_out);
				if(fd[0]!=STDIN_FILENO) close(fd[0]);
				if(fd[1]!=STDOUT_FILENO) close(fd[1]);			
			}
			int status;
			waitpid(last_cmd_pid,&status,0);
			cout << "success!" << endl;		
		}
		cout << "% ";

	}	
	return 0;
}

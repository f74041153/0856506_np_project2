# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <iostream>
# include <vector>
# include <map>
# include <string>
# include <sstream>
# include <sys/stat.h>
# include <unistd.h>

using namespace std;

map<string,string> env;

struct Pipe{
	int pipe_in;
	int pipe_out;
	int pipe_ctr;
};

struct CMD{
	vector<string> parsed_cmd;
	int N=0;
};


void my_setenv(string name,string value){
	env[name]=value;
//	cout << name << " " << value << endl;
}

void my_printenv(string name){
	if(env.find(name)!=env.end())
		cout << env[name] << endl;	
}

void my_exit(){
	exit(0);
}

vector<string> my_strtok(string str){
	vector<string> result;
	char* token = strtok(&str[0]," ");
	while(token != NULL){
		result.push_back(token);
		token = strtok(NULL," ");
	}
	//return result;

	stringstream ss(str);
	string tmp="";
	while(ss >> tmp){
		cout << tmp <<endl;
	}
	return result;

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

bool lineEndWithPipeN (vector<struct CMD> cmds){
	
	for(int i=0;i<cmds.size();i++){
		if(i+cmds[i].N+1>cmds.size()){
			return false;
		}	
	}
	return true;
}

bool fileExist(string filename){
	struct stat buf;
	return (stat(filename.c_str(),&buf) == 0);
}

void create_process(string cmd_path,string arg){
	cout << cmd_path << endl;
	cout << arg.c_str() << endl;
	pid_t pid = fork();
	if(pid < 0){
		exit(EXIT_FAILURE);
	}else if (pid == 0){
		if(execl(cmd_path.c_str(),cmd_path.c_str(),".",NULL)<0){
			exit(EXIT_FAILURE);
		}
	}
}

int main(){

	/* default path */
	my_setenv("PATH","bin:.");

	string line;
	cout << "% ";
	while(getline(cin,line)){
	
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
		

	//	string path = "bin/"+parsed_cmd[0][0];
	//	string arg = parsed_cmd[0].size()>1?parsed_cmd[0][1]:"";
	//	cout << arg << endl;
	//	if(fileExist(path)){
	//		create_process(path,arg);			
	//	}

		cout << "% ";

		/* handle other cmd */

	}	
	return 0;
}

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

vector<vector<string>> parse_cmd (string cmd){
	
	vector<vector<string>> parsed_cmd;
	stringstream ss1(cmd);
	string str;
	int pipe_site=0;
	vector<int> pipe_site_table;


	while(getline(ss1,str,'|')){
		
		stringstream ss2(str);
		vector<string> v;
		bool check_pipeN = true;
		
		while(ss2 >> str){
			if(check_pipeN && isNumber(str)){
				pipe_site_table.push_back(pipe_site);
				check_pipeN = false;	
			}
			v.push_back(str);
		}
		parsed_cmd.push_back(v);
		pipe_site++;

	}
	return parsed_cmd;
}

bool lineEndWithPipeN (vector<vector<string>>parsed_cmd){
	vector<int> pipe_cnt_table;

/*	for(int i=0;i<parsed_cmd.size();i++){
		
		int pipe_n = stoi(parsed_cmd[i][j]);
		if(pipe_n>=0 && pipe_n<=1000){
			if(parsed_cmd[i].size>1){
				pipe_cnt_table.push_back
			}
			pipe_cnt_table.push_back(pipe_n);
		}else{
			for(int k=0;k<pipe_cnt_table.size();k++){
				pipe_cnt_table[k]--;
			}
		}

	}
*/
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

	string cmd;
	cout << "% ";
	while(getline(cin,cmd)){
	
		/* handle input*/
		vector<vector<string>> parsed_cmd = parse_cmd(cmd);
		
		/* check whether is built-in command */
		if(!strcmp(parsed_cmd[0][0].c_str(),"setenv")){
			my_setenv(parsed_cmd[0][1],parsed_cmd[0][2]);
		}else if(!strcmp(parsed_cmd[0][0].c_str(),"printenv")){
			my_printenv(parsed_cmd[0][1]);
		}else if(!strcmp(parsed_cmd[0][0].c_str(),"exit")){
			my_exit();
		}
		

		string path = "bin/"+parsed_cmd[0][0];
		string arg = parsed_cmd[0].size()>1?parsed_cmd[0][1]:"";
	//	cout << arg << endl;
		if(fileExist(path)){
			create_process(path,arg);			
		}

		cout << "% ";

		/* handle other cmd */

	}	
	return 0;
}

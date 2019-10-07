# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <iostream>
# include <vector>
# include <map>
# include <string>

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
	return result;
}


int main(){

	/* default path */
	env["PATH"] = "bin:.";
	
	string cmd;
	cout << "% ";
	while(getline(cin,cmd)){
	
		/* handle input*/
		vector<string> splitted_cmd;
		splitted_cmd = my_strtok(cmd);

		/* check whether is built-in command */
		if(!strcmp(&splitted_cmd[0][0],"setenv")){
		//	cout << "setenv"<<endl;
			my_setenv(splitted_cmd[1],splitted_cmd[2]);
		}else if(!strcmp(&splitted_cmd[0][0],"printenv")){
		//	cout << "printenv" << endl;
			my_printenv(splitted_cmd[1]);
		}else if(!strcmp(&splitted_cmd[0][0],"exit")){
		//	cout << "exit" << endl;
			my_exit();
		}
		cout << "% ";

		// handle ou
	}	
	return 0;
}

# include <cstdio>
# include <cstdlib>
# include <cstring>
# include <iostream>
# include <vector>
# include <string>

using namespace std;

vector<string> path;

void my_setenv(){

}

void my_printenv(){
		
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
	
	path.push_back("bin");
	path.push_back(".");
	
	string cmd;
	cout << "% ";
	while(getline(cin,cmd)){
		vector<string> result;
		result = my_strtok(cmd);
		if(!strcmp(&result[0][0],"setenv")){
		//	cout << "setenv"<<endl;
		}else if(!strcmp(&result[0][0],"printenv")){
		//	cout << "printenv" << endl;
		}else if(!strcmp(&result[0][0],"exit")){
		//	cout << "exit" << endl;
			my_exit();
		}
		cout << "% ";
	}	
	return 0;
}

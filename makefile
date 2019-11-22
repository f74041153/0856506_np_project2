all: npshell.cpp np_simple.cpp np_single_proc.cpp np_multi_proc.cpp 
	g++ np_simple.cpp -o np_simple
	g++ np_single_proc.cpp -o np_single_proc
	g++ np_multi_proc.cpp -o np_multi_proc
clean: 
	rm -f np_simple
	rm -f np_single_proc
	rm -f np_multi_proc
np_simple: np_simple.cpp
	 g++ np_simple.cpp -o np_simple
np_single_proc: np_single_proc.cpp
	g++ np_single_proc.cpp -o np_single_proc
np_multi_proc: np_multi_proc.cpp
	g++ np_multi_proc.cpp -o np_multi_proc

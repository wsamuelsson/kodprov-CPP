all:	
	g++ middleware.cpp -o middleware.out -std=c++17 -pthread
clean:
	rm middleware.out
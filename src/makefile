TARGETS =  fdwrapper.o conn.o mgr.o springsnail

all : $(TARGETS)

fdwrapper.o : fdwrapper.cpp fdwrapper.h
	g++ -c fdwrapper.cpp -o fdwrapper.o

conn.o : conn.cpp conn.h
	g++ -c conn.cpp -o conn.o

mgr.o : mgr.cpp mgr.h
	g++ -c mgr.cpp -o mgr.o

springsnail : main.cpp processpool.h fdwrapper.o conn.o mgr.o
	g++ processpool.h fdwrapper.o conn.o mgr.o main.cpp -o springsnail

clean:
	rm -rf *.o springsnail
	
	

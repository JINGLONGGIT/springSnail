CC = g++
FLAG = -g

#TARGETS = demo_socket_server demo1_server demo1_client
TARGETS =  demo19_server stress_test

all : $(TARGETS)

stress_test : stress_test.cpp
	$(CC) -o stress_test stress_test.cpp

demo19_server : demo19_server.cpp
	$(CC) -o demo19_server demo19_server.cpp http_conn.cpp -lpthread

demo18_server : demo18_server.cpp
	$(CC) -o demo18_server demo18_server.cpp

demo17_server : demo17_server.cpp
	$(CC) -o demo17_server demo17_server.cpp
	
demo16 : demo16.cpp
	$(CC) -o demo16 demo16.cpp -lpthread

demo15 : demo15.cpp
	$(CC) -o demo15 demo15.cpp -lpthread

demo14_server : demo14_server.cpp
	$(CC) -o demo14_server demo14_server.cpp -lrt

demo13 : demo13.cpp
	$(CC) -o demo13 demo13.cpp

demo11_server : demo11_server.cpp
	$(CC) -o demo11_server demo11_server.cpp

demo10_client : demo10_client.cpp
	$(CC) -o demo10_client demo10_client.cpp

demo9_server : demo9_server.cpp
	$(CC) -o demo9_server demo9_server.cpp

demo8_server : demo8_server.cpp
	$(CC) -o demo8_server demo8_server.cpp

demo7_server : demo7_server.cpp
	$(CC) -o demo7_server demo7_server.cpp

demo6_server : demo6_server.cpp
	$(CC) -o demo6_server demo6_server.cpp

demo6_client : demo6_client.cpp
	$(CC) -o demo6_client demo6_client.cpp

demo5_server : demo5_server.cpp
	gcc -o demo5_server demo5_server.cpp -lpthread

demo4_server : demo4_server.cpp
	$(CC) -o demo4_server demo4_server.cpp

demo3_server : demo3_server.cpp
	$(CC) -o demo3_server demo3_server.cpp

demo2_server : demo2_server.cpp
	$(CC) -o demo2_server demo2_server.cpp

demo1_server : demo1_server.cpp
	$(CC) -o demo1_server demo1_server.cpp

demo1_client : demo1_client.cpp
	$(CC) -o demo1_client demo1_client.cpp

demo_socket_client : demo_socket_client.cpp
	$(CC) -o demo_socket_client demo_socket_client.cpp

demo_socket_server : demo_socket_server.cpp
	$(CC) -o demo_socket_server demo_socket_server.cpp

clean:
	rm -rf *.o *.a demo_socket_client demo_socket_server \
				demo1_client demo1_server demo2_server demo3_server demo4_server \
				demo5_server demo6_client demo6_server demo7_server demo8_server \
				demo9_server demo10_client demo11_server demo13 demo14_server \
				demo15 demo16 demo17_server demo19_server demo18_server stress_test
	
	

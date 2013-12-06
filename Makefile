
run: TCP_client TCP_server
	./select_side.sh 

TCP_client:	TCP_client.c
	gcc -Wall -g TCP_client.c -o TCP_client 

TCP_server:	TCP_server.c
	gcc -Wall -o  TCP_server TCP_server.c

TCP_server_linux: TCP_server.cpp
	g++ -Wall -I/home/seth/boost_1_54_0 TCP_server.cpp -o TCP_server_linux \
	/home/seth/boost_1_54_0/stage/lib/libboost_system.a

TCP_client_linux: TCP_client.cpp
	g++ -Wall -I/home/seth/boost_1_54_0 TCP_client.cpp -o TCP_client_linux \
	/home/seth/boost_1_54_0/stage/lib/libboost_system.a -lpthread

IP_Checksum:	IP_Checksum.cpp
	g++ -Wall IP_Checksum.cpp -o IP_Checksum

clean:  
	rm TCP_client TCP_server TCP_client_linux TCP_server_linux

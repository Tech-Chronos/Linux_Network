.PHONY:all
all:UdpClientMain UdpServerMain

UdpServerMain:UdpServerMain.cc
	g++ -o $@ $^ -std=c++14

UdpClientMain:UdpClientMain.cc
	g++ -o $@ $^ -std=c++14

.PHONY:clean
clean:
	rm -f UdpClientMain UdpServerMain
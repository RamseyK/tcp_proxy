/**
   tcp_proxy
   ProxyServer.h
   Copyright 2011 Ramsey Kant

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef PROXYSERVER_H_
#define PROXYSERVER_H_

#include <iostream>
#include <stdio.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <list>
#include <map>

#include "config.h"
#include "ByteBuffer.h"
#include "Client.h"
#include "ProxyClient.h"

#define SOCKET int
#define INVALID_SOCKET -1

using namespace std;

class ProxyServer {
    
private:
	bool canRun;
    SOCKET listenSocket; // Descriptor for the listening socket
    map<SOCKET, Client*> *clientMap; // Maps a Socket descriptor to a pointer to a Client object (connected clients only)
    struct sockaddr_in serverAddr; // Structure for the server address
    fd_set fd_master; // Master file descriptor set (listening socket + client sockets)
    fd_set fd_read; // FD set of sockets being read/operated on
    int fdmax; // Max FD number (max sockets handle)
    
private:
    bool initSocket(int port);
    void closeSockets();
    void acceptConnection();
    void disconnectClient(Client*);
    void handleClient(Client*);
    void sendData(Client*, ByteBuffer*);
    Client* getClient(SOCKET);
	Client* getProxyClientOwner(SOCKET);
    void handleData(Client*, ByteBuffer*);
    
public:
    ProxyServer();
    ~ProxyServer();
    void runServer();
    void stopServer() {
    	canRun = false;
    }

};

#endif

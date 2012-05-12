/**
   tcp_proxy
   ProxyServer.cpp
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

#include "ProxyServer.h"

/**
 * Server Constructor
 * Initialize state and server variables
 */
ProxyServer::ProxyServer() {
	canRun = false;
    listenSocket = INVALID_SOCKET;
    memset(&serverAddr, 0, sizeof(serverAddr)); // Clear the address struct
    
    // Zero the file descriptor sets
    FD_ZERO(&fd_master);
    FD_ZERO(&fd_read);
    
	// Instance the clientMap, relates Socket Descriptor to pointer to Client object
    clientMap = new map<SOCKET, Client*>();
}

/**
 * Server Destructor
 * Closes all active connections and deletes the clientMap
 */
ProxyServer::~ProxyServer() {
	if(listenSocket != INVALID_SOCKET)
		closeSockets();
    delete clientMap;
}

/**
 * Init Socket
 * Initialize the Server Socket by requesting a socket handle, binding, and going into a listening state
 *
 * @param port Port to listen on
 * @return True if initialization succeeded. False if otherwise
 */
bool ProxyServer::initSocket(int port) {
    // Request a handle for the listening socket, TCP
    listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listenSocket == INVALID_SOCKET){
        printf("ProxyServer: Could not create socket.\n");
        return false;
    }
 
    // Populate the server address structure
    serverAddr.sin_family = AF_INET; // Family: IP protocol
    serverAddr.sin_port = htons(port); // Set the port (convert from host to netbyte order
    serverAddr.sin_addr.s_addr = INADDR_ANY; // Let the OS select our address
    
    // Bind: assign the address to the socket
    if(bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) != 0){
        printf("ProxyServer: Failed to bind to the address\n");
        return false;
    }
    
    // Listen: put the socket in a listening state, ready to accept connections
    // (SOMAXCONN) Accept a backlog of the OS Maximum connections in the queue
    if(listen(listenSocket, SOMAXCONN) != 0){
        printf("ProxyServer: Failed to put the socket in a listening state\n");
        return false;
    }
    
    // Add the listenSocket to the master fd list
    FD_SET(listenSocket, &fd_master);
    // Set max fd to listenSocket
    fdmax = listenSocket;
    
    canRun = true;

    return true;
}

/**
 * Accept Connection
 * When a new connection is detected in runServer() this function is called. This attempts to accept the pending connection, instance a Client object, and add to the client Map
 */
void ProxyServer::acceptConnection() {
	// Setup new client variables
    sockaddr_in clientAddr;
    int clientAddrLen = sizeof(clientAddr);
    SOCKET clfd = INVALID_SOCKET;
    
    // Accept pending connection and retrieve the client socket descriptor.  Bail if accept failed.
    clfd = accept(listenSocket, (sockaddr*)&clientAddr, (socklen_t*)&clientAddrLen);
    if (clfd == INVALID_SOCKET)
        return;
    
    // Create a new Client object
    Client *cl = new Client(clfd, clientAddr);

	// Initiate the Proxy connection
	// If the ProxyClient failed to connect, reject this client's connection
	if(!cl->proxyConnect(PROXYCLIENT_HOST, PROXYCLIENT_PORT)) {
		printf("ProxyServer: New Client's ClientProxy couldn't connect to target host, booting client\n");
		close(clfd);
		delete cl;
		return;
	}
    
    // Add the client's socket and the proxyclient's socket to the master FD set
    FD_SET(clfd, &fd_master);
	FD_SET(cl->getProxySocket(), &fd_master);
    
    // If the client's handle is greater than the max, set the new max
    if(clfd > fdmax)
        fdmax = clfd;

    // If the proxyclient's handle is greater than the max, set the new max
    if(cl->getProxySocket() > fdmax)
        fdmax = cl->getProxySocket();
    
    // Add the client object to the client map
    clientMap->insert(pair<int, Client*>(clfd, cl));
    
    // Print connection message
    printf("ProxyServer: %s has connected\n", cl->getClientIP());
}

/**
 * Run Server
 * Main server loop where the socket is initialized and the loop is started, checking for new messages or clients to be read with select()
 * and handling them appropriately
 */
void ProxyServer::runServer() {
    //Initializing the socket
    if (!initSocket(PROXYSERVER_PORT)) {
        printf("ProxyServer: Failed to set up the server\n");
        return;
    }

	printf("ProxyServer: ProxyServer has started successfully!\n\n");

    while(canRun) {
		usleep(1000);

        // Copy the master set into fd_read for processing
        fd_read = fd_master;
        
        // Populate fd_read with client & clientProxy descriptors that are ready to be read
        if(select(fdmax+1, &fd_read, NULL, NULL, NULL) < 0)
            continue; // Nothing to be read
        
        // Loop through all the descriptors in both fd_read and fd_proxy_read sets and check to see if data needs to be processed
        for(int i=0; i <= fdmax; i++) {
            // Socket isnt in the read set, continue
            if(!FD_ISSET(i, &fd_read))
				continue;

			// If a socket is within the fd_read set, it's determine the type (client or proxyclient) and handle:

			// A new client is waiting to be accepted on the listenSocket
			if(i == listenSocket) {
				acceptConnection();
				continue;
			} 
				
			// If it's in the client map, handleClient. Otherwise it's a ProxyClient
			Client *cl = getClient(i);
			if(cl != NULL) {
				handleClient(getClient(i));
			} else {
				Client* cl = getProxyClientOwner(i);
				if(cl == NULL)
					continue; // Shouldn't ever happen...but just in case
				ProxyClient* pCl = cl->getProxyClient();

				// Run the ProxyClient processing method, if there is a returned ByteBuffer, forward that to the Client
				ByteBuffer* bfor = pCl->clientProcess();

				// Proxy Client is no longer connected, disconnect the Client
				if(!pCl->isClientRunning()) {
					disconnectClient(cl);
					continue;
				}

				// Data was recieved by the ProxyClient and needs to be passed onto the Client
				if(bfor != NULL) {
					sendData(cl, bfor);
					delete bfor;
				}
			}
        }
    }

    closeSockets(); //Closes all connections to the server
}

/**
 * Handle Client
 * Recieve data from a client that has indicated (via select()) that it has data waiting. Pass recv'd data to handleRequest()
 * Also detect any errors in the state of the socket
 *
 * @param cl Pointer to Client that sent the data
 */
void ProxyServer::handleClient(Client *cl) {
    if (cl == NULL)
        return;
    
    size_t dataLen = SO_RCVBUF;
    char *pData = new char[dataLen];
    
    // Receive data on the wire into pData
    /* TODO: Figure out what flags need to be set */
    int flags = 0; 
    ssize_t lenRecv = recv(cl->getSocket(), pData, dataLen, flags);
    
    // Determine state of client socket and act on it
    if(lenRecv == 0) {
        // Client closed the connection
        printf("ProxyServer: Client[%s] has disconnected\n", cl->getClientIP());
        disconnectClient(cl);
    } else if(lenRecv < 0) {
        // Some error occured
        disconnectClient(cl);
    } else {
		printf("ProxyServer: Recieved data of size %u\n", lenRecv);
        ByteBuffer *buf = new ByteBuffer((byte *)pData, (unsigned int)lenRecv);
        handleData(cl, buf);
        delete buf;
    }
    
    delete [] pData;
}

/**
 * Handle Data
 * Handle a Packet from a Client recv'd over the wire. Called from handleClient()
 *
 * @param cl Pointer to Client that sent the Packet
 * @param buf Pointer to ByteBuffer containing data recv'd
 */
void ProxyServer::handleData(Client *cl, ByteBuffer *buf) {
	// Simply forward the recieved data to the ProxyClient
	ProxyClient* pCl = cl->getProxyClient();
	pCl->sendData(buf);
}

/**
 * Send Data
 * Send Packet data to a paricular Client
 *
 * @param cl Client to send data to
 * @param buf ByteBuffer containing data to be sent
 */
void ProxyServer::sendData(Client* cl, ByteBuffer* buf) {
	size_t totalSent = 0, bytesLeft = buf->size(), dataLen = buf->size();
    ssize_t n = 0;

	// Get raw data
	byte* pData = new byte[dataLen];
	buf->getBytes(pData, dataLen);

	// Solution to deal with partials sends...loop till totalSent matches dataLen
	while(totalSent < dataLen) {
		n = send(cl->getSocket(), pData+totalSent, bytesLeft, 0);

		// Client closed the connection
		if(n < 0) {
			printf("ProxyServer: Client[%s] has disconnected\n", cl->getClientIP());
			disconnectClient(cl);
			break;
		}

		// Adjust byte count after a successful send
		totalSent += n;
		bytesLeft -= n;
	}
}

/**
 * Disconnect Client
 * Close the client's socket descriptor and release it from the FD map, client map, and memory
 *
 * @param cl Pointer to Client object
 */
void ProxyServer::disconnectClient(Client *cl) {
    if (cl == NULL)
        return;
    
	// Close the socket descriptor
    close(cl->getSocket());
	// Remove from the FD set (used in select())
    FD_CLR(cl->getSocket(), &fd_master);
	FD_CLR(cl->getProxySocket(), &fd_master);
	// Remove from the clientMap
    clientMap->erase(cl->getSocket());
    
	// Free client object from memory
    delete cl;
}

/**
 * Get Client
 * Lookup client based on the socket descriptor number in the clientMap
 *
 * @param clfd Client Descriptor
 * @return Pointer to Client object if found. NULL otherwise
 */
Client* ProxyServer::getClient(SOCKET clfd) {
	// Lookup in map
    map<int, Client*>::const_iterator it;
    it = clientMap->find(clfd);
    
    // Client wasn't found, return NULL
    if (it == clientMap->end())
        return NULL;
    
    // Return a pointer to the Client corresponding to the SOCKET that was found in clientMap
    return it->second;
}

/**
 * Get Proxy Client Owner
 * Given a Proxy Client's FD, grab the Client associated with it
 *
 * @param fd ProxyClient Descriptor
 * @return Pointer to Client object if found. NULL otherwise
 */
Client* ProxyServer::getProxyClientOwner(SOCKET fd) {
    map<int, Client*>::const_iterator it;
	Client* cl = NULL;
    for (it = clientMap->begin(); it != clientMap->end(); it++) {
        cl = it->second;
		if((cl != NULL) && (cl->getProxySocket() == fd))
			return cl;
    }
	return NULL;
}

/**
 * Close Sockets
 * Close all sockets found in the client map. Called on server shutdown
 */
void ProxyServer::closeSockets() {
	printf("ProxyServer: Closing all connections and shutting down the listening socket..\n");

	// Loop through all client's in the map and disconnect them
    map<int, Client*>::const_iterator it;
    for (it = clientMap->begin(); it != clientMap->end(); it++) {
        Client *cl = it->second;
        disconnectClient(cl);
    }
    
    // Clear the map
    clientMap->clear();
    
    // Shutdown the listening socket
    shutdown(listenSocket, SHUT_RDWR);
    
    // Release the listenSocket to the OS
    close(listenSocket);
    listenSocket = INVALID_SOCKET;
}




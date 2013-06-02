/**
   tcp_proxy
   ProxyClient.cpp
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

#include "ProxyClient.h"

/**
 * Client Constructor
 * Initializes default values for private members
 *
 */
ProxyClient::ProxyClient() {
    clientSocket = INVALID_SOCKET;
	host = "";
	port = 443;
	clientRunning = false;

	// Zero out the address hints structure
    memset(&hints, 0, sizeof(hints));
}

/**
 * Client Destructor
 */
ProxyClient::~ProxyClient() {
	if(clientSocket != INVALID_SOCKET)
		disconnect();
}

/**
 * InitSocket
 * Initialize the socket and put it in a state thats ready for connect()
 *
 * @param host Server host string
 * @param port Server port to connect to (default is 443)
 * @return True on success, false otherwise
 */
bool ProxyClient::initSocket(string h, int p) {
    // Setup the address structure
	host = h;
	port = p;
	char portstr[8];
	sprintf(portstr, "%i", port);
    hints.ai_family = AF_UNSPEC; // Will be determined by getaddrinfo (IPv4/v6)
	hints.ai_socktype = SOCK_STREAM;
	getaddrinfo(host.c_str(), portstr, &hints, &res);

    // Get a handle for clientSocket
    clientSocket = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if(clientSocket == INVALID_SOCKET) {
        printf("ProxyClient: Could not create socket handle");
        return false;
    }

	// At this point, initilization succeeded so return the socket handle
	return clientSocket;
}

/**
 * Connect
 * Attempt to connect to the target host. Do NOT call if initSocket() failed
 *
 * @return Socket handle is returned upon successful connection. -1 (INVALID_SOCKET) on failure
 */
SOCKET ProxyClient::attemptConnect() {
	// Attempt to connect to the server
	printf("ProxyClient: Attempting to connect to %s:%i...\n", host.c_str(), port);
    int cret = 0;
	cret = connect(clientSocket, res->ai_addr, res->ai_addrlen);
	if(cret < 0) {
		printf("ProxyClient: Connect failed!\n");
		close(clientSocket);
		return INVALID_SOCKET;
	}

	printf("ProxyClient: Connection was successful!\n");

	// Set as non blocking
	//fcntl(clientSocket, F_SETFL, O_NONBLOCK);

	// Connect was successful, so return the socket handle
	clientRunning = true;
	return clientSocket;
}

/**
 * Client Process
 * Runs the main checks for new packets
 *
 * Return's a ByteBuffer is new data was recieved
 */
ByteBuffer* ProxyClient::clientProcess() {
	size_t dataLen = SO_RCVBUF;
	char *pData = new char[dataLen];
	ByteBuffer *retBuf = NULL;
    
	// Receive data on the wire into pData
	/* TODO: Figure out what flags need to be set */
	int flags = 0; 
	ssize_t lenRecv = recv(clientSocket, pData, dataLen, flags);
    
	// Act on return of recv. 0 = disconnect, -1 = no data, else the size to recv
	if(lenRecv == 0) {
		// Server closed the connection
		printf("ProxyClient: Socket closed by server, disconnecting\n");
		clientRunning = false;
	} else if(lenRecv == -1) {
		// No data to recv. Empty case to trap
	} else {
		printf("ProxyClient: Recieved data of size %u\n", lenRecv);
		// Usable data was received. Create a new instance of a ByteBuffer, pass it the data from the wire
        ByteBuffer *buf = new ByteBuffer((byte *)pData, (unsigned int)lenRecv);

		// Delete pData from memory, no longer needed
		delete [] pData;

		// Pass the new ByteBuffer into the handler, return the output of the handler
		retBuf = handleData(buf);
	}

	if(pData != NULL)
		delete [] pData;

	return retBuf;
}

/**
 * Handle Data
 * Accepts an incomming packet from the server, parses it, and returns the processed data back as a ByteBuffer. Called from clientProcess()
 *
 * @param buf Pointer to the bytebuffer recieved on the wire
 * @return A processed ByteBuffer (SSL decrypted)
 */
ByteBuffer* ProxyClient::handleData(ByteBuffer *buf) {
	// Do processing here

	return buf;
}

/**
 * Send Data
 * Complete a send over the wire to a server.
 *
 * @param pkt Pointer to an instance of a ByteBuffer to send over the wire
 */
void ProxyClient::sendData(ByteBuffer *buf){
	size_t totalSent = 0, bytesLeft = buf->size(), dataLen = buf->size();
    ssize_t n = 0;
	
	// Get raw data
	byte* pData = new byte[dataLen];
	buf->getBytes(pData, dataLen);

	// Do any processing here

	// Solution to deal with partials sends...loop till totalSent matches dataLen
	while(totalSent < dataLen) {
		n = send(clientSocket, pData+totalSent, bytesLeft, 0);

		// Server closed the connection
		if(n < 0) {
			printf("ProxyClient: Error in sending data, socket closed, disconnecting\n");
			clientRunning = false;
			break;
		}

		// Adjust byte count after a successful send
		totalSent += n;
		bytesLeft -= n;
	}
}

/**
 * Disconnect
 * Shutdown and close the socket handle, clean up any other resources in use
 */
void ProxyClient::disconnect() {
	// Free the address structure
	freeaddrinfo(res);

	// Shutdown and close the socket, then set in an invalid state
	shutdown(clientSocket, SHUT_RDWR);
	close(clientSocket);
    clientSocket = INVALID_SOCKET;

	printf("ProxyClient: Client has disconnected from the server.\n");
}


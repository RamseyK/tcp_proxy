/**
   tcp_proxy
   main.cpp
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

#include <signal.h>
#include "ProxyServer.h"

ProxyServer *svr;

// Handles an unix terminiation signals (Ctrl C)
void sighandler(int sig) {
	svr->stopServer();
}

int main (int argc, const char * argv[])
{
	// Register sighandler for terminiation signals:
	signal(SIGABRT, &sighandler);
	signal(SIGINT, &sighandler);
	signal(SIGTERM, &sighandler);

	// Instance and start the proxy server
    svr = new ProxyServer();
    svr->runServer();
    delete svr;



    return 0;
    
}

#include "thread.h"
#include "socketserver.h"
#include <algorithm>
#include <stdlib.h>
#include <time.h>
#include "Semaphore.h"
#include <list>
#include <vector>
#include <thread>
#include <cmath>

using namespace Sync;

class SocketThread : public Thread
{
private:
    // Reference to connected socket
    Socket &socket;
    // A byte array for the data we are receiving and sending
    ByteArray data;
	// Global indicator of number of game rooms
	int gameRoomNum;
	// The port the server is running on
	int port;
    // Reference to exit variable. If false, terminate the threads
    bool& exitBool;
    // Container for the SocketThread pointers
    std::vector<SocketThread*> &socketThreadHolder;
    
public:
	SocketThread(Socket& socket, std::vector<SocketThread*> &clientSocketThread, bool &exitBool, int port) :
		socket(socket), socketThreadHolder(clientSocketThread), exitBool(exitBool), port(port)
	{}	// Constructor, initialize values

    ~SocketThread()
    {
		this->terminationEvent.Wait();
	}

    Socket& GetSocket()
    {
        return socket;
    }

    const int GetGameRoom()
    {
        return gameRoomNum;
    }

    virtual long ThreadMain()
    {
		try {
			gameRoomNum = ceil(socketThreadHolder.size() / 2.0); // Get game room num
			std::cout << "Client connected, current game room number: " << gameRoomNum << std::endl; // Send on first connect

			// Set p1 or p2
			if (socketThreadHolder.size() % 2 == 0) {
				Socket &clientSocket = this->GetSocket();
				ByteArray playerNum("p2");
				clientSocket.Write(playerNum);
			}
			else {
				Socket &clientSocket = this->GetSocket();
				ByteArray playerNum("p1");
				clientSocket.Write(playerNum);
			}

			while(!exitBool) {
				int socketResult = socket.Read(data);
				// If the socket is closed on the client side, break from loop for this thread
				if (socketResult == 0)	break;

				std::string recv = data.ToString();
				if(recv == "shutdown") {

					// Remove threads
					socketThreadHolder.erase(std::remove(socketThreadHolder.begin(), socketThreadHolder.end(), this), socketThreadHolder.end());

					std::cout<< "A client has disconnected from the server, removing client..." << std::endl;
					break;
				}

				// Send data to other clients, only send if they are in the same room as this thread and not this thread
				for (int i = 0; i < socketThreadHolder.size(); i++) {
					SocketThread *clientSocketThread = socketThreadHolder[i];
					if (clientSocketThread->GetGameRoom() == gameRoomNum && this != clientSocketThread)
					{
						Socket &clientSocket = clientSocketThread->GetSocket();
						ByteArray sendBa(recv);
						clientSocket.Write(sendBa);
					}
				}
			}
		} 
		// Catch string thrown exceptions
		catch(std::string &s) {
			std::cout << s << std::endl;
		}
		// Catch thrown exceptions
		catch(std::exception &e){
			std::cout << "A client has abruptly quit!" << std::endl;
		}
		std::cout << "A client has left!" << std::endl;
		return 0;
	}
};

class ServerThread : public Thread
{
private:
	// Reference to socket server
    SocketServer &server;
    std::vector<SocketThread*> sThreadList;
	// Port number
	int port;
	// Exit variable
    bool exitBool = false;
    
public:
    ServerThread(SocketServer& server, int port)
    : server(server), port(port)
    {}

    ~ServerThread()
    {
        // Close the client sockets
        for (auto thread : sThreadList)
        {
            try
            {
                // Close the socket
                Socket& toClose = thread->GetSocket();
                toClose.Close();
            }
            catch (...)
            {
                // Catch all exceptions
            }
        }
		std::vector<SocketThread*>().swap(sThreadList);
        exitBool = true;
    }

    virtual long ThreadMain()
    {
        while (true)
        {
            try {
				// Convert port to string
                std::string stringPortNum = std::to_string(port);

				// Client receives welcome message through the socket (server)
				std::string welcomeMessage = "Welcome, connection to server successful!";
				// Byte array conversion for message
                ByteArray welcomeMessage_conv(welcomeMessage);
                // Wait for a client socket connection
                Socket sock = server.Accept();

				// Send welcome message
                sock.Write(welcomeMessage_conv);
                Socket* newConnection = new Socket(sock);
                // Pass a reference to this pointer into a new socket thread
                Socket &socketReference = *newConnection;
                sThreadList.push_back(new SocketThread(socketReference, std::ref(sThreadList), exitBool, port));
            }
			// Catch string thrown exceptions
            catch (std::string error)
            {
                std::cout << "ERROR: " << error << std::endl;
				// Exit thread function
                return 1;
            }
			// In case of unexpected shutdown
			catch (TerminationException terminationException)
			{
				std::cout << "Server has shut down!" << std::endl;
				// Exit with exception thrown
				return terminationException;
			}
			catch (...) {

			}
        }
    }
};

int main(void) {
	// Port info
    int port = 3005;

	// Startup message
    std::cout << "Pong Server" << std::endl 
		<<"Press enter to end server..." << std::endl;
		
	// Create the server on port specified
    SocketServer server(port);
	
	// Create server thread
    ServerThread st(server, port);

	// Wait for input to shutdown the server
	FlexWait cinWaiter(1, stdin);
	cinWaiter.Wait();
	std::cin.get();

	// Shut down and clean up the server
	server.Shutdown();

    std::cout << "Server shutting down" << std::endl;
}

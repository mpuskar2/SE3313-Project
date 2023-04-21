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
    // Reference to our connected socket.
    Socket &socket;
    // A byte array for the data we are receiving and sending.
    ByteArray data;
	// Global indicator of number of chat rooms.
	int chatRoomNum;
	// The port our server is running on.
	int port;
    // Reference to exit variable. If false, we terminate the threads.
    bool& exitBool;
    // Holder for SocketThread pointers.
    std::vector<SocketThread*> &socketThreadHolder;

    
public:
	SocketThread(Socket& socket, std::vector<SocketThread*> &clientSocketThread, bool &exitBool, int port) :
		socket(socket), socketThreadHolder(clientSocketThread), exitBool(exitBool), port(port)
	{}	//constructor - initialize our values

    ~SocketThread()
    {
		this->terminationEvent.Wait();
	}

    Socket& GetSocket()
    {
        return socket;
    }

    const int GetChatRoom()
    {
        return chatRoomNum;
    }

    virtual long ThreadMain()
    {
		try {
			// Attempt to gather bytestream data.
			// socket.Read(data);

			// std::string chatRoomString = data.ToString();
			// std::cout << socketThreadHolder.size() << std::endl; // print data to console
			// chatRoomString = chatRoomString.substr(1, chatRoomString.size() - 1);
			// chatRoomNum = std::stoi(chatRoomString);

			chatRoomNum = ceil(socketThreadHolder.size() / 2.0);
			std::cout << socketThreadHolder.size() << std::endl;
			std::cout << "Current chat room number from socket.Read(): " << chatRoomNum << std::endl;	//send this on first connect

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
				// If the socket is closed on the client side, exitBool this socket thread.
				if (socketResult == 0)	break;

				std::string recv = data.ToString();
				if(recv == "shutdown") {

					// Remove threads
					socketThreadHolder.erase(std::remove(socketThreadHolder.begin(), socketThreadHolder.end(), this), socketThreadHolder.end());

					std::cout<< "A client is shutting off from our server. Erase client!" << std::endl;
					break;
				}

				// A forward slash is appended as the first character to change the chat room number.
				if (recv[0] == '/') {
					// Split the received string.
					std::string stringChat = recv.substr(1, recv.size() - 1);
				
					// Parse the integer after the forward slash character, representing the chat room number.
					chatRoomNum = std::stoi(stringChat);
					std::cout << "A client just joined room " << chatRoomNum << std::endl;
					continue;
				}

				for (int i = 0; i < socketThreadHolder.size(); i++) {
					SocketThread *clientSocketThread = socketThreadHolder[i];
					if (clientSocketThread->GetChatRoom() == chatRoomNum && this != clientSocketThread)
					{
						Socket &clientSocket = clientSocketThread->GetSocket();
						ByteArray sendBa(recv);
						clientSocket.Write(sendBa);
					}
				}
			}
		} 
		// catch any exceptions by strings 
		catch(std::string &s) {
			std::cout << s << std::endl;
		}
		// Catch thrown exceptions and distinguish them in console.
		catch(std::exception &e){
			std::cout << "A client has abruptly quit their messenger app!" << std::endl;
		}
		std::cout << "A client has left!" << std::endl;
		return 0;
	}
};

class ServerThread : public Thread
{
private:
	// Reference to socket server.
    SocketServer &server;
    std::vector<SocketThread*> sThreadList;
	// port number
	int port;
	//number of rooms on server to chat on
	int rooms;
	// exit variable
    bool exitBool = false;
    
public:
    ServerThread(SocketServer& server, int rooms, int port)
    : server(server), rooms(rooms), port(port)
    {}

    ~ServerThread()
    {
		
        // Close the client sockets.
        for (auto thread : sThreadList)
        {
            try
            {
                // Close the socket.
                Socket& toClose = thread->GetSocket();
                toClose.Close();
            }
            catch (...)
            {
                // catch all exceptions
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
				// convert port to string
                std::string stringPortNum = std::to_string(port);
                std::cout << "FlexWait/Natural blocking call on client!" <<std::endl;

				// client receives welcome message through the socket (server)
				std::string welcomeMessage = "Welcome, connection to server successful!";
				// Byte array conversion for message
                ByteArray welcomeMessage_conv(welcomeMessage);
                // Wait for a client socket connection
                Socket sock = server.Accept();

				// Send welcome message
                sock.Write(welcomeMessage_conv);
                Socket* newConnection = new Socket(sock);
                // Pass a reference to this pointer into a new socket thread.
                Socket &socketReference = *newConnection;
                sThreadList.push_back(new SocketThread(socketReference, std::ref(sThreadList), exitBool, port));
            }
			// Catch string-thrown exception.
            catch (std::string error)
            {
                std::cout << "ERROR: " << error << std::endl;
				// Exit thread function.
                return 1;
            }
			// In case of unexpected shutdown.
			catch (TerminationException terminationException)
			{
				std::cout << "Server has shut down!" << std::endl;
				// Exit with exception thrown.
				return terminationException;
			}
			catch (...) {

			}
        }
    }
};

int main(void) {
	//port info
    int port = 3005;
	// Admin sets value of number of chat rooms for the server.
    int rooms = 5;

    std::cout << "Group 14 Super Awesome Fun Sweet And Cool Server" << std::endl 
		<<"Press enter to end server..." << std::endl;
		
	//create the server on port specified
    SocketServer server(port);
	
	// create server thread with specified nmber of rooms
    ServerThread st(server, rooms, port);

	// This will wait for input to shutdown the server
	FlexWait cinWaiter(1, stdin);
	cinWaiter.Wait();
	std::cin.get();


	// Shut down and clean up the server
	server.Shutdown();

    std::cout << "Good-bye!" << std::endl;
}

#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "interface.h"

#include <string>
#include <sstream>
#include <arpa/inet.h>
#include <sstream>

#include <iostream>
#include <thread>

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::stringstream;



int connect_to(const char *host, const int port);
struct Reply process_command(const int sockfd, char* command);
void process_chatmode(const char* host, const int port);

int main(int argc, char** argv) 
{
	if (argc != 3) {
		fprintf(stderr,
				"usage: enter host address and port number\n");
		exit(1);
	}

    display_title();
    
	while (1) {
	
		int sockfd = connect_to(argv[1], atoi(argv[2]));
		if (sockfd < 0){
			break;
		}
		
		char command[MAX_DATA];
        get_command(command, MAX_DATA);

		struct Reply reply = process_command(sockfd, command);
		display_reply(command, reply);
		
		touppercase(command, strlen(command) - 1);
		if (strncmp(command, "JOIN", 4) == 0 && reply.status != FAILURE_NOT_EXISTS) {
			printf("Now you are in the chatmode\n");
			process_chatmode(argv[1], reply.port);
		}
	
		close(sockfd);
    }

    return 0;
}

/*
 * Connect to the server using given host and port information
 *
 * @parameter host    host address given by command line argument
 * @parameter port    port given by command line argument
 * 
 * @return socket fildescriptor
 */
int connect_to(const char *host, const int port)
{
	/*
	// ------------------------------------------------------------
	// GUIDE :
	// In this function, you are suppose to connect to the server.
	// After connection is established, you are ready to send or
	// receive the message to/from the server.
	// 
	// Finally, you should return the socket fildescriptor
	// so that other functions such as "process_command" can use it
	// ------------------------------------------------------------

    // below is just dummy code for compilation, remove it.
    */
    //create socket
	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1){
		return -1;
	}
	
	//create hint structure
	sockaddr_in hint;
	hint.sin_family = AF_INET; //using IPv4
	hint.sin_port = htons(port); //attatch port num
	inet_pton(AF_INET, host, &hint.sin_addr);
	
	//connect to server 
	int conn = connect(sockfd, (sockaddr*)&hint, sizeof(hint));
	if (conn == -1){
		return -2;
	}
	
	
	return sockfd;
}

string toString(char* c){
	int i = 0;
	string newString = "";
	while (c[i] != '\0'){
		newString += c[i];
		i++;
	}
	
	return newString;
}

/* 
 * Send an input command to the server and return the result
 *
 * @parameter sockfd   socket file descriptor to commnunicate
 *                     with the server
 * @parameter command  command will be sent to the server
 *
 * @return    Reply    
 */
struct Reply process_command(const int sockfd, char* command)
{
	/**
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse a given command
	// and create your own message in order to communicate with
	// the server. Surely, you can use the input command without
	// any changes if your server understand it. The given command
    // will be one of the followings:
	//
	// CREATE <name>
	// DELETE <name>
	// JOIN <name>
    // LIST
	//
	// -  "<name>" is a chatroom name that you want to create, delete,
	// or join.
	// 
	// - CREATE/DELETE/JOIN and "<name>" are separated by one space.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 2:
	// After you create the message, you need to send it to the
	// server and receive a result from the server.
	// ------------------------------------------------------------


	// ------------------------------------------------------------
	// GUIDE 3:
	// Then, you should create a variable of Reply structure
	// provided by the interface and initialize it according to
	// the result.
	//
	// For example, if a given command is "JOIN room1"
	// and the server successfully created the chatroom,
	// the server will reply a message including information about
	// success/failure, the number of members and port number.
	// By using this information, you should set the Reply variable.
	// the variable will be set as following:
	//
	// Reply reply;
	// reply.status = SUCCESS;
	// reply.num_member = number;
	// reply.port = port;
	// 
	// "number" and "port" variables are just an integer variable
	// and can be initialized using the message fomr the server.
	//
	// For another example, if a given command is "CREATE room1"
	// and the server failed to create the chatroom becuase it
	// already exists, the Reply varible will be set as following:
	//
	// Reply reply;
	// reply.status = FAILURE_ALREADY_EXISTS;
    // 
    // For the "LIST" command,
    // You are suppose to copy the list of chatroom to the list_room
    // variable. Each room name should be seperated by comma ','.
    // For example, if given command is "LIST", the Reply variable
    // will be set as following.
    //
    // Reply reply;
    // reply.status = SUCCESS;
    // strcpy(reply.list_room, list);
    // 
    // "list" is a string that contains a list of chat rooms such 
    // as "r1,r2,r3,"
	// ------------------------------------------------------------
	**/
	// REMOVE below code and write your own Reply.
	
	//going to use the commmand line input with no changes
	
	//send message to server
	char buf[4096]; //buffer for the command from the user
	char recvBuf[4096];

	//convert command to a string in order to manipulate easier
	string command_string = toString(command);
	stringstream grabComm(command_string);
	string commType;
	grabComm >> commType;
	for (int i = 0; i < commType.size(); i++){
	commType[i] =toupper(commType[i]);
	}
	
	//send command to server
	int sendResult = send(sockfd, command, 4096, 0);
	
	//wait for response
	memset(recvBuf, 0, 4096);
	int bytesRecv = recv(sockfd, recvBuf, 4096, 0);
	
	
	// format reply
	struct Reply reply;
	
	
	// if there was an eror with the connection
	if (bytesRecv == -1){
		reply.status = FAILURE_UNKNOWN;
		return reply;
	}
	
	//create a string stream for parsing response
	string servResp = toString(recvBuf);
	std::stringstream ss(servResp);
	string servComm;
	ss >> servComm;

	
	
	// If the input is invalid, have server return "invalid"
	if (servComm == "INVALID"){
		reply.status = FAILURE_INVALID;
	}
	//room exists already
	else if(servComm == "EXISTS"){
		reply.status = FAILURE_ALREADY_EXISTS;
	}	
	//room doesnt exist
	else if(servComm == "NOROOM"){
		reply.status = FAILURE_NOT_EXISTS;
	}
	// CREATE
	else if (servComm == "CREATE"){
		reply.status = SUCCESS;
		ss >> reply.num_member >> reply.port;
		
	}
	// DELETE
	else if (servComm == "DELETE"){
		reply.status = SUCCESS;
		reply.num_member = 0;
	}
	// JOIN
	else if (servComm == "JOIN"){
		reply.status = SUCCESS;
		ss >> reply.num_member >> reply.port;
	}
	// LIST
	else if (servComm == "LIST"){

		reply.status = SUCCESS;
		string roomList;
		ss >> roomList;
		strcpy(reply.list_room, roomList.c_str());
		
	}
	

	return reply;
}

void recv_thread(int sockfd);
void send_thread(int sockfd);

/* 
 * Get into the chat mode
 * 
 * @parameter host     host address
 * @parameter port     port
 */
void process_chatmode(const char* host, const int port)
{
	/*
	// ------------------------------------------------------------
	// GUIDE 1:
	// In order to join the chatroom, you are supposed to connect
	// to the server using host and port.
	// You may re-use the function "connect_to".
	// ------------------------------------------------------------

	// ------------------------------------------------------------
	// GUIDE 2:
	// Once the client have been connected to the server, we need
	// to get a message from the user and send it to server.
	// At the same time, the client should wait for a message from
	// the server.
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    // 1. To get a message from a user, you should use a function
    // "void get_message(char*, int);" in the interface.h file
    // 
    // 2. To print the messages from other members, you should use
    // the function "void display_message(char*)" in the interface.h
    //
    // 3. Once a user entered to one of chatrooms, there is no way
    //    to command mode where the user  enter other commands
    //    such as CREATE,DELETE,LIST.
    //    Don't have to worry about this situation, and you can 
    //    terminate the client program by pressing CTRL-C (SIGINT)
	// ------------------------------------------------------------
	*/
	//connect to the chat server
	int chat_socket = connect_to(host, port);
	if (chat_socket < 0) {}
	else{
		
		//receive and send threads
		std::thread rec(recv_thread, chat_socket);
		std::thread send(send_thread, chat_socket);
		
		rec.join();
		close(chat_socket);
		exit(0);
		send.join();
	}
	    
}
	
void recv_thread(int sockfd){
	while(1){
		//Recieve Chats
	    char buf[4096];
		memset(buf, 0, 4096);		
		int bytesRecv = recv(sockfd, buf, 4096, 0);
		
		if (bytesRecv == 0) {
			cout << "> Warnning: the chatting room is going to be closed...";
			break;
		}
		if (toString(buf) == "~CHATCLOSED~"){
			break;
		}
		display_message(buf);
		cout << "\n";
			
		
	    
	}
}

void send_thread(int sockfd){
	while(1){
		//Send Chat
		char buf[4096];
		memset(buf, 0, 4096);
		get_message(buf, 4096);
		size_t msg_len = strlen(buf);
		if (!msg_len) continue;
		int sendRes = send(sockfd, buf, 4096, 0);
	}
}
	



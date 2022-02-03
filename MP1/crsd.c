#include <iostream>
#include <sys/types.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>
#include <string>
#include <sys/select.h>
#include <sys/time.h>
#include <sstream>
#include <unordered_map>
#include <signal.h>
#include <set>
#include <vector>



using namespace std;

//chat room structure
struct chat_room {
    int portNo;
    string name;
    int numMems;
    int masterSocket = -1;
    vector<int> userfds;
};


unordered_map<string, chat_room> chat_rooms; //list of chat rooms key: name
unordered_map<int, string> fd_name; //list of fds for chat rooms



int cur_chat_port = 9000;

int setup_server(int port);

int accept_connection(int server_socket);

string interpret_command(string comm, int server_socket, fd_set& current_sockets, int& joinSock, string& chatName);


int main(int argc, char** argv) 
{
	if (argc != 2) {
		fprintf(stderr,
				"usage: Enter a port number\n");
		exit(1);
	}
	
	int PORTNO = atoi(argv[1]);
    
    //initialize the server
    int server_socket = setup_server(PORTNO);
    
    //set up the fd set
    fd_set current_sockets, ready_sockets;
    
    FD_ZERO(&current_sockets);
    FD_SET(server_socket, &current_sockets);
    
    int maxSocket = server_socket+1;
    
    while(true){
        ready_sockets = current_sockets;

        if (select(maxSocket, &ready_sockets, nullptr, nullptr, nullptr) < 0){
            return -5;
        }
        
        //loop through all possible fds in select
        for (int i = 0; i <= maxSocket; i++){
            if (FD_ISSET(i, &ready_sockets)){
                
                //if the fd is the server socket, accept new client
                if (i == server_socket){
                    int client_socket = accept_connection(server_socket);
                        if (client_socket != -1){
                        FD_SET(client_socket, &current_sockets);
                        maxSocket = max(maxSocket, client_socket+1);
                    }
                    
                }
                
                //if the fd is one of the ones in a chat, handle the recv and do not close
                else if (fd_name.find(i) != fd_name.end()){
                    char buf[4096];
                    memset(buf, 0, 4096);
                    
                    int recvResult = recv(i, buf, 4096, 0);
                    
                    //client leaves server
                    if (recvResult == 0){

                        string roomName = fd_name[i];
                        chat_rooms[roomName].numMems--;
                        
                        //remove person who left from chat rooms stucture
                        for (int j = 0; j < chat_rooms[roomName].userfds.size(); j++){
                            if (chat_rooms[roomName].userfds.at(j) == i){
                                chat_rooms[roomName].userfds.erase(chat_rooms[roomName].userfds.begin()+j);
                                break;
                            } 
                            
                        }
                        fd_name.erase(i);
                        close(i);
                        FD_CLR(i, &current_sockets);
                    } 
                    //client sends message to chat
                    else{
                        
                        string msg = string(buf, 0, recvResult);
                        string roomName = fd_name[i];
                        
                        //send message to everyone in the chat
                        for (int j = 0; j < chat_rooms[roomName].userfds.size(); j++){
                            if (chat_rooms[roomName].userfds[j] != i){
                                send(chat_rooms[roomName].userfds[j], msg.c_str(), msg.size() + 1, 0);
                            } 
                        }
                    }
                    
                    
                }
                //if the client is in command mode
                else{
                    char buf[4096];
                    memset(buf, 0, 4096);
                    
                    //receive clients command and handle accordingly
                    int recvResult = recv(i, buf, 4096, 0);
                    string command = string(buf, 0, recvResult);
                    int joinSock = 0;
                    string chatName = "";
                    string response = interpret_command(command, server_socket, current_sockets, joinSock, chatName);
                    send(i, response.c_str(), response.size() + 1, 0);
                    
                    //if the command was a join, accept the new client and add to room
                    if (response.substr(0,4) == "JOIN"){
                        
                        int client_chat_sock = accept_connection(joinSock);
                        if (client_chat_sock != -1){
                            FD_SET(client_chat_sock, &current_sockets);
                            maxSocket = max(maxSocket, client_chat_sock+1);
                            fd_name[client_chat_sock] = chatName;
                            chat_rooms[chatName].userfds.push_back(client_chat_sock);
                        } 
                        
                        
                    }

                    close(i);
                    FD_CLR(i, &current_sockets);
                }
            }
        }
        
    }
    
    
    
    close(server_socket);
    
    return 0;
    
}

int setup_server(int port){
    int server_socket, client_socket, addr_size;

    //create socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1){
        return -1;
    }
    sockaddr_in hint;
    hint.sin_family = AF_INET;
    hint.sin_port = htons(port); //converts to little endian
    inet_pton(AF_INET, "127.0.0.1", &hint.sin_addr);
    
    //bind server to socket and IP
    if (bind(server_socket, (sockaddr*)&hint, sizeof(hint)) == -1){
        return -2;
    }
    
    //mark socket for listenign in
    if (listen(server_socket, SOMAXCONN) == -1){
        return -3;
    }
    

    return server_socket;
    
}

int accept_connection(int server_socket){
    
    //create socket address for client
    sockaddr_in client;
    socklen_t clientsize = sizeof(client);

    
    int clientSocket = accept(server_socket, (sockaddr*)&client, &clientsize);
    if (clientSocket == -1){
        return -1;
    }
    

    return clientSocket;
}


string interpret_command(string comm, int server_socket, fd_set& current_sockets, int& join, string& chatName){
    
    //interpret clients commnand
    stringstream ss(comm);
    
    string typeComm;
    string interpreted;
    
    
    ss >> typeComm;
    for(int i = 0; i < typeComm.size(); i++) typeComm[i] = toupper(typeComm[i]);
    
    //command create
    if (typeComm == "CREATE"){
        string roomName;
        ss >> roomName;
        
        //room name not entered
        if (roomName.size() == 0){
            return "INVALID";
        }
        //room already exists
        else if(chat_rooms.find(roomName) != chat_rooms.end()){
            return "EXISTS";
        }
        //room is created
        else{
            //add room to current chat rooms and return the port and the number of members
            //create master socket for the new chat
            int chat_socket = setup_server(cur_chat_port);

            //add master socket to the list of current sockets
            FD_SET(chat_socket, &current_sockets);
            
            //store a chat room with appropriate values
            chat_rooms[roomName] = {cur_chat_port, roomName, 0, chat_socket, {}};
            
            
            interpreted = "CREATE 0 " + to_string(cur_chat_port);
            
            //set of master sockets
            fd_name[chat_socket] = roomName;
            cur_chat_port++;
            return interpreted;
        }
    }
    //command JOIN
    else if (typeComm == "JOIN"){
        string roomName;
        ss >> roomName;
        
        //room not entered
        if (roomName.size() == 0){
            return "INVALID";
        }
        //room doesnt exist
        else if(chat_rooms.find(roomName) == chat_rooms.end()){
            return "NOROOM";
        }
        //room exists
        else{
            chat_rooms[roomName].numMems++;
            auto& room = chat_rooms[roomName];
            
            interpreted = "JOIN " + to_string(room.numMems) + " " + to_string(room.portNo);
            join = room.masterSocket;
            chatName = roomName;
            return interpreted;
        }
        
    }
    //command list
    else if (typeComm == "LIST"){
        
        if (chat_rooms.size() == 0) return "LIST no_chat_rooms";
        
        interpreted = "LIST ";
        for (auto chats : chat_rooms){
            interpreted += chats.first + ",";
        }
        return interpreted;
    }
    //command delete
    else if (typeComm == "DELETE"){
        string roomName;
        ss >> roomName;
        
        //room not entered
        if (roomName.size() == 0){
            return "INVALID";
        }
        //room doesnt exist
        else if(chat_rooms.find(roomName) == chat_rooms.end()){
            return "NOROOM";
        }
        //room exists
        else{
            //go through it member in the chat and close their connection
            for(int i = 0; i < chat_rooms[roomName].userfds.size(); i++){

                    fd_name.erase(chat_rooms[roomName].userfds.at(i));
                    FD_CLR(chat_rooms[roomName].userfds.at(i), &current_sockets);
                    close(chat_rooms[roomName].userfds.at(i));
            }
            //close master chat socket
            FD_CLR(chat_rooms[roomName].masterSocket, &current_sockets);
            fd_name.erase(chat_rooms[roomName].masterSocket);
            close(chat_rooms[roomName].masterSocket);
            chat_rooms.erase(roomName);
            
            return "DELETE";
        }
        
    
    }
    
    return "INVALID";
}

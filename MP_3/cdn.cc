#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <ctime>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>

#include "sns.grpc.pb.h"

using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using csce438::Reply;
using csce438::ReqMaster;
using csce438::RetMaster;
using csce438::SNSService;
using csce438::hb;
using csce438::newSlave;
using csce438::fmsg;
using csce438::frep;


using std::string;
using std::cout;
using std::endl;

// struct for the routing tables
struct routing {
    
    string serv_id;
    string spno;
    string ip;
    bool status;
    std::time_t lastTime;
    
    routing()
    : serv_id(""), spno(""), ip(""), status(false), lastTime(0) {};
    
    routing(string serv_id, string spno, string ip, bool status)
    : serv_id(serv_id), spno(spno), ip(ip), status(status) {};
    
};

routing master_routing[3];
routing slave_routing[3];
routing fs_routing[3];

class SNSServiceImpl final : public SNSService::Service {
    
    routing getServer(string clientID){
        
        int serverID = std::stoi(clientID) % 3;
        
        if (master_routing[serverID].status)
            return master_routing[serverID];
        else
            cout << "Master down. Switched to slave\n";
            return slave_routing[serverID];
        
    }
    
    routing getFsync(string clientID){
        return fs_routing[std::stoi(clientID) % 3];
    }
  
  // protbuf functions
    Status master(ServerContext* context, const ReqMaster* request, RetMaster* reply) override {
      
      // read in the clients ID
      string clientID = request->id();

      
      // grab server for client
      routing server = getServer(clientID);
      //special case
      if (server.status == false){
            std::cerr << "server for this id has not been made yet, please try again!\n";
            reply->set_ip("");
            reply->set_pno("");
      }
      
      reply->set_ip(server.ip);
      reply->set_pno(server.spno);
      
      return Status::OK;
  }
  
    Status heartbeat(ServerContext* context, const hb* request, newSlave* reply) override {
      
      // read in the clients ID
      string serverID = request->id();
      
      // if the id is not in the routing table yet, add it
      // master
      if (request->t() == "master"){
          
          if (master_routing[std::stoi(serverID) - 1].status){

              master_routing[std::stoi(serverID) - 1].lastTime = time(0);
          }
          
          else {

              master_routing[std::stoi(serverID) - 1].serv_id = serverID;
              master_routing[std::stoi(serverID) - 1].spno = request->pno();
              master_routing[std::stoi(serverID) - 1].ip = request->ip();
              master_routing[std::stoi(serverID) - 1].status = true;
              master_routing[std::stoi(serverID) - 1].lastTime = time(0);

          }
      }
      //slave
      else {
          
          if (slave_routing[std::stoi(serverID) - 1].status){
              slave_routing[std::stoi(serverID) - 1].lastTime = time(0);
          }
          
          else {
              slave_routing[std::stoi(serverID) - 1].serv_id = serverID;
              slave_routing[std::stoi(serverID) - 1].spno = request->pno();
              slave_routing[std::stoi(serverID) - 1].ip = request->ip();
              slave_routing[std::stoi(serverID) - 1].status = true;
              slave_routing[std::stoi(serverID) - 1].lastTime = time(0);
              
          }
      }
      // send to client the master server for it
      if (slave_routing[std::stoi(serverID) - 1].status){
          reply->set_isslave("1");
          reply->set_spno(slave_routing[std::stoi(serverID) - 1].spno) ;
      }else{
          reply->set_isslave("0");
      }
      
      return Status::OK;
  }

    Status updateSync(ServerContext* context, const fmsg* request, frep* reply) override {
      
      // read in the fs and add it to the list if it is not already there
      
      int curID = std::stoi(request->id());
      

      if (fs_routing[curID - 1].serv_id == ""){

          fs_routing[curID - 1].serv_id = request->id();
          fs_routing[curID - 1].spno = request->pno();
          fs_routing[curID - 1].ip = request->ip();
          fs_routing[curID - 1].status = true;
      }
      

      int fsc = 0;
      for (int i = 0; i < 3; i++){
          if ((curID-1) != i){
              if (fs_routing[i].serv_id != ""){
                reply->add_id(fs_routing[i].serv_id);
                reply->add_pno(fs_routing[i].spno);
                reply->add_ip(fs_routing[i].ip);
                fsc++;
              }
          }
      }
      reply->set_fscount(std::to_string(fsc));
      
      return Status::OK;
  }
  
};

void checkMaster(){
    

    
    while(true){
        

        

        for (int i = 0; i < 3; i++){
            if (master_routing[i].lastTime < (time(0) - 20)){

                master_routing[i].status = false;
            }
        }
        
        sleep(10);
    }
}

void RunCoordinator(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;

  server->Wait();
}

int main(int argc, char** argv) {
  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  std::thread check(checkMaster);
  
  RunCoordinator(port);
  
  check.join();

  return 0;
}
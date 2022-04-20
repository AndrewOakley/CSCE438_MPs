/*
 *
 * Copyright 2015, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <thread>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <mutex>
#include <sstream>
#include <set>

#include "sns.grpc.pb.h"

#include <sys/types.h>
#include <sys/stat.h>




using google::protobuf::Timestamp;
using google::protobuf::Duration;
using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerReader;
using grpc::ServerReaderWriter;
using grpc::ServerWriter;
using grpc::Status;
using grpc::ClientContext;
using csce438::Message;
using csce438::ListReply;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;
using csce438::ReqMaster;
using csce438::RetMaster;
using csce438::hb;
using csce438::newSlave;
using csce438::NewMessage;

std::string serv_type = "master";
std::unique_ptr<SNSService::Stub> slave_stub_ = nullptr;
std::string thisID = "none";
std::set<std::string> otherUsers;
std::time_t prevUpdate = 0;

//Vector that stores every client that has been created

struct Client {
  std::string username;
  bool connected = true;
  int following_file_size = 0;
  std::vector<Client*> client_followers;
  std::vector<Client*> client_following;
  std::set<std::string> outFollow;
  std::set<std::string> inFollow;
  ServerReaderWriter<Message, Message>* stream = 0;
  bool operator==(const Client& c1) const{
    return (username == c1.username);
  }
};

std::vector<Client> client_db;

void checkForUpdates(){
  
  while(true){

  std::ifstream updFile;
  updFile.open("fs_" + serv_type + "_" + thisID + "/update.txt");
  
  if (updFile.is_open()){
    std::string curLine;
    std::time_t maxTime = prevUpdate;
    while(std::getline(updFile, curLine)){
      

      if (curLine.size() == 0) break;
      

      std::stringstream ss(curLine);
      
      std:time_t logTime;
      std::string user1;
      std::string logType;
        ss >> logTime >> user1 >> logType;
      
      if (logTime > prevUpdate){
        maxTime = std::max(maxTime, logTime);
      
        
        // login
        if (logType == "LOGIN"){
          if (otherUsers.find(user1) == otherUsers.end()){

            otherUsers.insert(user1);
          }
        }
        
        // follow
        else if (logType == "FOLLOWED"){
          // TODO add follower and adjust list
          std::string user2;
          ss >> user2;
          for (int j = 0; j < client_db.size(); j++){
            if (client_db.at(j).username == user2 && client_db.at(j).inFollow.find(user1) == client_db.at(j).inFollow.end())
              client_db.at(j).inFollow.insert(user1);
          }
        }
        // timeline
        else if (logType == "TIMELINE"){
          
          std::string timelineTime;
          ss >> timelineTime;

          std::string holdPost;
          std::string post;
          while (ss >> holdPost){
            post += holdPost + " ";
          }
          post.pop_back();
          
          for (Client& c : client_db){
            
            // loop through all clients to find clients that follow this user

              
              // if they follow then send the post to the user
              if (c.outFollow.find(user1) != c.outFollow.end()){
                
                if (c.stream != 0){
                  //stream->
                  Message server_resp;
                  server_resp.set_username(user1);
                  server_resp.set_msg(post);
                  
                  Timestamp* ts = server_resp.mutable_timestamp();
                  google::protobuf::util::TimeUtil::FromString(timelineTime, ts);
                  
                  c.stream->Write(server_resp);
                  
                  //std::string filename = username+".txt";
                  //std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
                  //google::protobuf::Timestamp temptime = message.timestamp();
                  //std::string time_ = google::protobuf::util::TimeUtil::ToString(temptime);
                  //std::string fileinput = time_+" :: "+message.username()+":"+message.msg()+"\n";
                }
                //else{
                
                
                if (serv_type == "slave"){
                // place in following txt
                  std::string fileinput = timelineTime+" :: "+user1+":"+post+"\n";
                  std::string temp_username = c.username;
                  std::string temp_file = temp_username + "following.txt";
                	std::ofstream following_file(temp_file,std::ios::app);
  	              following_file << fileinput;
                  c.following_file_size++;
                  following_file.close();
                }
                //}
              }
            
          }
        }
      
      }
      
      
    }  
    prevUpdate = maxTime;
        
  }
  updFile.close();
  sleep(10);
  }
}

void updateUserLog(std::string userID, std::string log_info, std::mutex& mtx){
  
  std::ofstream logFile;
  std::string logDir;
  
  if (userID == "-1"){
      mtx.lock();
      logDir = serv_type + "_" + thisID + "/loginLog.txt";
      mtx.unlock();
  }else{
      logDir = serv_type + "_" + thisID + "/t"+userID+".txt";
  }


  logFile.open(logDir, std::ios::app);
  logFile << time(0) << " " << log_info << std::endl;
  
  logFile.close();

}

void updateFollowLog(std::string userID, std::string log_info, std::mutex& mtx){
  
  std::ofstream logFile;
  std::string logDir = serv_type + "_" + thisID + "/followerInfo.txt";
  
  mtx.lock();

  logFile.open(logDir, std::ios::app);
  logFile << time(0) << " " << log_info << std::endl;
  
  logFile.close();

  mtx.unlock();
}




//Helper function used to find a Client object given its username
int find_user(std::string username){
  int index = 0;
  for(Client c : client_db){
    if(c.username == username)
      return index;
    index++;
  }
  return -1;
}

class SNSServiceImpl final : public SNSService::Service {
  
  std::mutex mtx;
  
  Status List(ServerContext* context, const Request* request, ListReply* list_reply) override {
    Client user = client_db[find_user(request->username())];
    int index = 0;
    for(Client c : client_db){
      list_reply->add_all_users(c.username);
    }
    for (std::string s : otherUsers){
      list_reply->add_all_users(s);
    }
    std::vector<Client*>::const_iterator it;
    for(it = user.client_followers.begin(); it!=user.client_followers.end(); it++){
      list_reply->add_followers((*it)->username);
    }
    for (std::string str : user.inFollow){
      list_reply->add_followers(str);
    }
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int join_index = find_user(username2);
    
    if (otherUsers.find(username2) != otherUsers.end()){
      if (client_db[find_user(username1)].outFollow.find(username2)
          != client_db[find_user(username1)].outFollow.end()){
        reply->set_msg("you have already joined");
          return Status::OK; 
        }else{
          reply->set_msg("Follow Successful");
          client_db[find_user(username1)].outFollow.insert(username2);
          std::string logUpdate = username1 + " FOLLOWED " + username2;
          updateFollowLog(username1, logUpdate, mtx);
        }
    }else{
    if((join_index < 0 || username1 == username2))
      reply->set_msg("unkown user name");

    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[join_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) != user1->client_following.end()){
	reply->set_msg("you have already joined");
        return Status::OK;
      }
      user1->client_following.push_back(user2);
      user2->client_followers.push_back(user1);
      reply->set_msg("Follow Successful");
      
          // log information in file
      std::string logUpdate = username1 + " FOLLOWED " + username2;
      updateFollowLog(username1, logUpdate, mtx);
      

    }
    }
    

    if (serv_type == "master" && slave_stub_ != nullptr){
        Request slaveReq;
        slaveReq.set_username(request->username());
        slaveReq.add_arguments(request->arguments(0));
        Reply slaveRep;
        ClientContext slaveCont;
        slave_stub_->Follow(&slaveCont, slaveReq, &slaveRep);
    }

    
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    std::string username1 = request->username();
    std::string username2 = request->arguments(0);
    int leave_index = find_user(username2);
    if(leave_index < 0 || username1 == username2)
      reply->set_msg("Leave Failed -- Invalid Username");
    else{
      Client *user1 = &client_db[find_user(username1)];
      Client *user2 = &client_db[leave_index];
      if(std::find(user1->client_following.begin(), user1->client_following.end(), user2) == user1->client_following.end()){
	reply->set_msg("Leave Failed -- Not Following User");
        return Status::OK;
      }
      user1->client_following.erase(find(user1->client_following.begin(), user1->client_following.end(), user2)); 
      user2->client_followers.erase(find(user2->client_followers.begin(), user2->client_followers.end(), user1));
      reply->set_msg("Leave Successful");
    }
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    Client c;
    std::string username = request->username();
    int user_index = find_user(username);
    if(user_index < 0){
      c.username = username;
      client_db.push_back(c);
      reply->set_msg("Login Successful!");
    }
    else{ 
      Client *user = &client_db[user_index];
      if(user->connected)
        reply->set_msg("Invalid Username");
      else{
        std::string msg = "Welcome Back " + user->username;
	reply->set_msg(msg);
        user->connected = true;
      }
    }
    if (serv_type == "master" && slave_stub_ != nullptr){
      ClientContext slaveCont;
      Request slaveReq;
      slaveReq.set_username(username);
      Reply slaveRep;
      

      slave_stub_->Login(&slaveCont, slaveReq, &slaveRep);
    }

    std::string logUpdate = username + " LOGIN";
    updateUserLog("-1", logUpdate, mtx);
    
    return Status::OK;
  }

  Status Timeline(ServerContext* context, 
		ServerReaderWriter<Message, Message>* stream) override {
    Message message;
    Client *c;
    while(stream->Read(&message)) {
      
      if (message.msg() != "Set Stream" && serv_type == "master" && slave_stub_ != nullptr){
        NewMessage slaveMsg;
        slaveMsg.set_username(message.username());
        slaveMsg.set_sender("");
        slaveMsg.set_msg(message.msg());
        google::protobuf::Timestamp* slaveTS = new google::protobuf::Timestamp();
        google::protobuf::Timestamp temptime = message.timestamp();
        slaveTS->set_seconds(temptime.seconds());
        slaveTS->set_nanos(0);

        slaveMsg.set_allocated_timestamp(slaveTS);
        Reply slaveRep;
        ClientContext slaveCont;
        slave_stub_->updateTimeline(&slaveCont, slaveMsg, &slaveRep);
      }
      
      std::string username = message.username();
      int user_index = find_user(username);
      c = &client_db[user_index];
 
      //Write the current message to "username.txt"
      std::string filename = username+".txt";
      std::ofstream user_file(filename,std::ios::app|std::ios::out|std::ios::in);
      google::protobuf::Timestamp temptime = message.timestamp();
      std::string time_ = google::protobuf::util::TimeUtil::ToString(temptime);
      std::string fileinput = time_+" :: "+message.username()+":"+message.msg();

      
      //"Set Stream" is the default message from the client to initialize the stream
      if(message.msg() != "Set Stream"){
        user_file << fileinput;
        std::string logUpdate = username + " TIMELINE " + time_ + " " + message.msg();
        logUpdate.pop_back();
        updateUserLog(username, logUpdate, mtx);
      }
      //If message = "Set Stream", print the first 20 chats from the people you follow
      else{
        if(c->stream==0)
      	  c->stream = stream;
        std::string line;
        std::vector<std::string> newest_twenty;
        std::ifstream in(username+"following.txt");
        int count = 0;
        //Read the last up-to-20 lines (newest 20 messages) from userfollowing.txt
        while(getline(in, line)){
          if(c->following_file_size > 20){
	    if(count < c->following_file_size-20){
              count++;
	      continue;
            }
          }
          newest_twenty.push_back(line);
        }
        Message new_msg; 
 	//Send the newest messages to the client to be displayed
	for(int i = 0; i<newest_twenty.size(); i++){
	  new_msg.set_msg(newest_twenty[i]);
          stream->Write(new_msg);
        }    
        continue;
      }
      //Send the message to each follower's stream
      std::vector<Client*>::const_iterator it;
      for(it = c->client_followers.begin(); it!=c->client_followers.end(); it++){
        Client *temp_client = *it;
      	if(temp_client->stream!=0 && temp_client->connected){
      	  std::string temps = message.msg();
      	  temps.pop_back();
      	  message.set_msg(temps);
	  temp_client->stream->Write(message);
      	}
	  
	  
	      // add to all other users timelines
	      std::string logUpdate = username + " TIMELINE " + time_ + " " + message.msg();
        logUpdate.pop_back();
        updateUserLog(temp_client->username, logUpdate, mtx);
        
        if (message.msg() != "Set Stream" && serv_type == "master" && slave_stub_ != nullptr){
          NewMessage slaveMsg;
          slaveMsg.set_username(message.username());
          slaveMsg.set_sender(temp_client->username);
          slaveMsg.set_msg(message.msg());
          google::protobuf::Timestamp* slaveTS = new google::protobuf::Timestamp();
          google::protobuf::Timestamp temptime = message.timestamp();
          slaveTS->set_seconds(temptime.seconds());
          slaveTS->set_nanos(0);
  
          slaveMsg.set_allocated_timestamp(slaveTS);
          Reply slaveRep;
          ClientContext slaveCont;
          slave_stub_->updateTimeline(&slaveCont, slaveMsg, &slaveRep);
        }
        
        
        //For each of the current user's followers, put the message in their following.txt file
        std::string temp_username = temp_client->username;
        std::string temp_file = temp_username + "following.txt";
	std::ofstream following_file(temp_file,std::ios::app|std::ios::out|std::ios::in);
	following_file << fileinput;
        temp_client->following_file_size++;
	std::ofstream user_file(temp_username + ".txt",std::ios::app|std::ios::out|std::ios::in);
        user_file << fileinput;
      }
      
    
      
    }
    

    //If the client disconnected from Chat Mode, set connected to false
    c->connected = false;
    return Status::OK;
  }

  Status updateTimeline(ServerContext* context, const NewMessage* message, Reply* reply) override {
    
    std::string logUpdate = message->username() + " TIMELINE " +  google::protobuf::util::TimeUtil::ToString(message->timestamp()) + " " + message->msg();
    logUpdate.pop_back();
    if (message->sender() != "")
      updateUserLog(message->sender(), logUpdate, mtx);
    else 
      updateUserLog(message->username(), logUpdate, mtx);
    
    return Status::OK;
  }
};

void RunServer(std::string port_no) {
  std::string server_address = "0.0.0.0:"+port_no;
  SNSServiceImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl;
  
  server->Wait();
}

void connect_to_slave(std::string port){
    std::string login_info = "0.0.0.0:" + port;
    slave_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));

    
}

void heartBeat(std::string id, std::string cpno, std::string cip, std::string serv_type,
      std::string port, std::string ip){
      // connect to coordinator
    // create coordinator stub
    std::string login_info = cip + ":" + cpno;
    std::unique_ptr<SNSService::Stub> cdn_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
                    

    bool connected_slave = false;
    
    while (true){
      hb request;
      request.set_id(id);
      request.set_t(serv_type);
      request.set_pno(port);
      request.set_ip(ip);
      
      newSlave reply;
      ClientContext context;
      Status status = cdn_stub_->heartbeat(&context, request, &reply);
      
      if (!connected_slave && serv_type == "master"){
        if (reply.isslave() == "1"){
          connect_to_slave(reply.spno());
          connected_slave = true;
        }
      }
      
      sleep(10);
    }

    
}

int main(int argc, char** argv) {
  
  std::string cip = "0.0.0.0";
  std::string cpno = "8080";
  std::string port = "8081";
  std::string id = "temporary";

  
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:p:c:i:t:")) != -1){
    switch(opt) {
      case 'h':
          cip = optarg;break;
      case 'p':
          port = optarg;break;
      case 'c':
          cpno = optarg;break;
      case 'i':
          id = optarg;break;
      case 't':
          serv_type = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  thisID = id;
  
  // create directory for persistency 
  std::string persPath = serv_type + "_" + id;
  if (mkdir(persPath.c_str(), 0777) == -1){
    
    if( errno == EEXIST ) {
       // alredy exists
    }
  }
  
  std::thread cdn_heartbeat (heartBeat, id, cpno, cip, serv_type, port, "0.0.0.0");
  std::thread check (checkForUpdates);
  
  RunServer(port);
  
  
  cdn_heartbeat.join();
  check.join();

  return 0;
}

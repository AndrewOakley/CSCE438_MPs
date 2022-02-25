#include <ctime>

#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>

#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <stdlib.h>
#include <unistd.h>
#include <google/protobuf/util/time_util.h>
#include <grpc++/grpc++.h>
#include <map>
#include <list>
#include <mutex>

#include <iostream>
#include <fstream>
#include <ctime>
#include <set>

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
using csce438::Message;
using csce438::Request;
using csce438::Reply;
using csce438::SNSService;


//struct for timelines
struct timelinePost{
  std::string username;
  std::time_t timestamp;
  std::string post;
  timelinePost(std::string u, std::time_t t, std::string p) 
    : username(u), timestamp(t), post(p) {};
    
  
};

//struct that holds who the user is following and when they started following
struct userFollowing {
  std::string user;
  std::time_t timestamp;
};

//struct for users
struct userStruct{
  
  std::vector<std::string> followers;
  std::vector<userFollowing> following;
  std::list<timelinePost> userTimeline;
  userStruct() {};
  
  void addPost(timelinePost post){
    if (userTimeline.size() == 20){
      userTimeline.pop_front();
      userTimeline.push_back(post);
    }else{
      userTimeline.push_back(post);
    }
  }
  
};

//map holding all users
std::map<std::string, userStruct> active_users;

//persistent timeline feature
//updates the database everytime a change is made
void updateFile(){
  std::ofstream timelineFile;
  timelineFile.open("timelineFile.txt", std::ios::out | std::ios::trunc);
  
  if (timelineFile.is_open()){
      
    for(auto curUser : active_users){
      timelineFile << curUser.first << " \n"; //grab username
      
      timelineFile << "-followers- \n";
      
      for (const std::string& f : curUser.second.followers){
        timelineFile << f << " \n";
      }
      
      timelineFile << "-following- \n";
      
      for (const userFollowing& f : curUser.second.following){
        timelineFile << f.user << " " << f.timestamp << " \n";
      }
      timelineFile << "-timelines- \n";
      
      for (timelinePost userPosts : curUser.second.userTimeline){
        timelineFile << userPosts.username << " \n";
        timelineFile << userPosts.timestamp << " \n";
        timelineFile << userPosts.post << "\n";
      }
    timelineFile << "--- \n";
    }
    
  
  }
  timelineFile.close();
}

//at launch, the server uploads any saved information from the data base for timeliens
void uploadFile(){
  std::ifstream ifs;
  ifs.open("timelineFile.txt");
  
  while (ifs.is_open() && !ifs.eof()){
    
    std::string username;
    ifs >> username;
    if (username.size() == 0) break;
    active_users[username] = userStruct();
    
    
    std::string cur;
    ifs >> cur; //discard -followers-
    
    ifs >> cur;
    while(cur != "-following-"){
      active_users[username].followers.push_back(cur);
      ifs >> cur;
    }
    
    
    while(true){
      std::string user;
      std::time_t fTime;
      
      ifs >> user;
      if (user == "-timelines-") break;
      ifs >> fTime;
      active_users[username].following.push_back({user, fTime});
      
    }

    while (1){
      std::string user;
      ifs >> user;
      if (user == "---"){
        break;
      }
      std::time_t ts;
      std::string p;
      std::string throwaway;
      ifs >> ts;
      getline(ifs, throwaway);
      getline(ifs, p);
      
      timelinePost temp(user, ts, p);
      active_users[username].addPost(temp);

      
    }
    
    
    
  }
  
  ifs.close();
}



class SNSServiceImpl final : public SNSService::Service {
  
  std::mutex mtx;
  
  //stores the streams for users in timeline mode
  std::map<std::string, std::set<ServerReaderWriter<Message, Message>*>> userStreams;
  
  Status List(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // LIST request from the user. Ensure that both the fields
    // all_users & following_users are populated
    // ------------------------------------------------------------
    
    std::string username = request->username();
    
    mtx.lock();
    
    userStruct curUser = active_users[username];
    
    //iterate through all users
    for (auto i : active_users) reply->add_all_users(i.first);
    
    //iterate through current users followers
    for (std::string follower : curUser.followers) reply->add_following_users(follower);
    
    mtx.unlock();
    
    return Status::OK;
  }

  Status Follow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to follow one of the existing
    // users
    // ------------------------------------------------------------
    std::string username = request->username();
    std::string other_user = request->arguments()[0];
    
    
    //user does not exist
    mtx.lock();
    if (active_users.find(other_user) == active_users.end()){
      mtx.unlock();
      reply->set_msg("NOUSER");
      return Status::OK;
    }
    //user does exist
    //make sure the user is not already following them
    bool already = false;
    for (const userFollowing& i : active_users[username].following) already = (i.user == other_user) || already;
    if (!already) {

      active_users[username].following.push_back({other_user, time(0)});
      active_users[other_user].followers.push_back(username);
      
      updateFile();
    } if (already){
      reply->set_msg("ALREADY");
    }
    mtx.unlock();

    
    
    return Status::OK; 
  }

  Status UnFollow(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // request from a user to unfollow one of his/her existing
    // followers
    // ------------------------------------------------------------
    std::string username = request->username();
    std::string other_user = request->arguments()[0];
    
    mtx.lock();
    //user does not exist
    if (active_users.find(other_user) == active_users.end()){
      mtx.unlock();
      reply->set_msg("NOUSER");
      return Status::OK;
    }
    //user does exist
    //make sure the user is not already following them
    bool following = false;
    for (const userFollowing& i : active_users[username].following) following = (i.user == other_user) || following;
    if (following) {
      for (int i = 0; i < active_users[username].following.size(); i++){
        if (active_users[username].following[i].user == other_user){
          active_users[username].following.erase(active_users[username].following.begin()+i);
        }
      }
      active_users[other_user].followers.erase(std::remove(active_users[other_user].followers.begin(),
            active_users[other_user].followers.end(), username), active_users[other_user].followers.end());
            
            updateFile();
    }
    //not even following
    else{
      reply->set_msg("ALREADY");
    }
    mtx.unlock();
    
    return Status::OK;
  }
  
  Status Login(ServerContext* context, const Request* request, Reply* reply) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // a new user and verify if the username is available
    // or already taken
    // ------------------------------------------------------------
    
    std::string username = request->username();
    
    mtx.lock();
    //user not exist
    if (active_users.find(username) == active_users.end()){
      active_users[username] = userStruct();
      
      //test cases say that user follows themselves
      active_users[username].following.push_back({username, time(0)});
      active_users[username].followers.push_back(username);
      
      updateFile();
      
    }else{
      //user already logged in
      reply->set_msg("ALREADY");
    }
    mtx.unlock();
    
    return Status::OK;
  }

  Status Timeline(ServerContext* context, ServerReaderWriter<Message, Message>* stream) override {
    // ------------------------------------------------------------
    // In this function, you are to write code that handles 
    // receiving a message/post from a user, recording it in a file
    // and then making it available on his/her follower's streams
    // ------------------------------------------------------------
    
    Message client_msg;
    Message server_resp;
    std::string username;
    
    //setup the user timeline    
    stream->Read(&client_msg);
    username = client_msg.username();
    
    //when timeline is first called, show user the posts they have missed
    if (client_msg.msg() == "adduser"){
      mtx.lock();
      
      //user is not already in stream list
      if (userStreams.find(username) == userStreams.end()){
        userStreams[username] = {stream};
      }else{
        userStreams[username].insert(stream);
      }

      
      //list of all posts that user is following
      std::vector<timelinePost> followedPosts;
      for (const userFollowing& i : active_users[username].following){
        std::string followName = i.user;
        std::time_t followTime = i.timestamp;
        
        
        for (timelinePost p : active_users[followName].userTimeline){
          if (p.timestamp >= followTime){
            
            followedPosts.push_back(p);
          }
        }

      }
      mtx.unlock();
      
      //comparator for posts
      auto comp = [](const timelinePost& lhs, const timelinePost& rhs){
        return lhs.timestamp > rhs.timestamp;
      };
      
      //sort and display post from most recent to least
      sort(followedPosts.begin(), followedPosts.end(), comp);
      
      for (timelinePost tp : followedPosts){
        server_resp.set_username(tp.username);
        server_resp.set_msg(tp.post);
        Timestamp* ts = server_resp.mutable_timestamp();
        *ts = google::protobuf::util::TimeUtil::TimeTToTimestamp(tp.timestamp);
        
        stream->Write(server_resp);
      }
    }

    //update the timeline
    while(stream->Read(&client_msg)){
      
      //read in post and add it to the users posts
      std::string getPost = client_msg.msg();
      getPost.pop_back();
      std:time_t tempTime = time(0);
      timelinePost temp(username, tempTime, getPost);
      
      //format response
      Timestamp* ts = server_resp.mutable_timestamp();
      *ts = google::protobuf::util::TimeUtil::TimeTToTimestamp(tempTime);
      server_resp.set_username(username);
      server_resp.set_msg(getPost);
      
      mtx.lock();
      active_users[username].addPost(temp);
      
      //send post to all users followers who are in the stream
      for (std::string f : active_users[username].followers){
        
        if (userStreams.find(f) != userStreams.end()){
          for (auto& s : userStreams[f])
            s->Write(server_resp);
        }
      }
      updateFile();
      mtx.unlock();
      
      //stream->Write(server_resp);

      //stream->WriteDone();
    }
    //The user has left the server
    //Therefore remove them from the list of streams
    
    //user only has 1 stream
    if (userStreams[username].size() == 1){
      userStreams.erase(username);
    }
    //user has multiple streams
    else{
      userStreams[username].erase(stream);
    }
    return Status::OK;
  }

};

void RunServer(std::string port_no) {
  // ------------------------------------------------------------
  // In this function, you are to write code 
  // which would start the server, make it listen on a particular
  // port number.
  // ------------------------------------------------------------
  
  //format server address and create instance of server
  std::string server_address("127.0.0.1:");
  server_address += port_no;
  SNSServiceImpl service;
  
  //setup server
  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  std::unique_ptr<grpc::Server> server(builder.BuildAndStart());
  std::cout << "Server listening on " << server_address << std::endl; //delete later
  uploadFile();
  server->Wait();
  
}

int main(int argc, char** argv) {

  
  std::string port = "3010";
  int opt = 0;
  while ((opt = getopt(argc, argv, "p:")) != -1){
    switch(opt) {
      case 'p':
          port = optarg;
          break;
      default:
	         std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
  RunServer(port);
  return 0;
}

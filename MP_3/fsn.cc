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
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <map>
#include <sstream>
#include <mutex>
#include <dirent.h>

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
using grpc::ClientContext;
using csce438::SNSService;
using csce438::fuser;

using std::string;
using std::cout;
using std::endl;

std::string id = "none";

struct fs {
    
    string id;
    string ip;
    string pno;
    fs(string id, string ip, string pno)
    : id(id), ip(ip), pno(pno) {};
    
    // for testing
    void printfs(){
        cout << id << "-" << ip << "-" << pno << endl;
    }
};

std::vector<fs> fsyncs;

class SNSServiceImpl final : public SNSService::Service {
    
    std::mutex mtx;
    
    Status sendUser(ServerContext* context, const fuser* request, Reply* reply) override {

        
        std::ofstream updFile;
        std::string updDir = "fs_master_" + id + "/update.txt";
        
        updFile.open(updDir, std::ios::app);
        
        if (request->t() == "LOGIN"){
            updFile << time(0) << " " << request->id(0) << " LOGIN\n";
        }
        // follow
        else if (request->t() == "FOLLOW"){
            updFile << time(0) << " " << request->id(0) << " FOLLOWED " << request->id(1) << endl;
        }
        //timeline
        else if (request->t() == "TIMELINE"){
            updFile << time(0) << " " << request->id(0) << " TIMELINE " << request->post() << endl;
        }
        
        updFile.close();
        
        updDir = "fs_slave_" + id + "/update.txt";
        
        updFile.open(updDir, std::ios::app);
        
        if (request->t() == "LOGIN"){
            updFile << time(0) << " " << request->id(0) << " LOGIN\n";
        }
        // follow
        else if (request->t() == "FOLLOW"){
            updFile << time(0) << " " << request->id(0) << " FOLLOWED " << request->id(1) << endl;
        }
        //timeline
        else if (request->t() == "TIMELINE"){
            updFile << time(0) << " " << request->id(0) << " TIMELINE " << request->post() << endl;
        }
        
        updFile.close();
        return Status::OK;
  }
};


void updateFS(string cpno, string cip, string port){
    
    
    std::string login_info = cip + ":" + cpno;
    std::unique_ptr<SNSService::Stub> cdn_stub_ = std::unique_ptr<SNSService::Stub>(SNSService::NewStub(
               grpc::CreateChannel(
                    login_info, grpc::InsecureChannelCredentials())));
                    

    
    while (true){
        
    
        // if all the fsyncs have been given no more need to do this
        if (fsyncs.size() == 2){
            break;
        }
        
        // set up a request for updateFsync
        fmsg request;
        request.set_id(id);
        request.set_pno(port);
        request.set_ip("0.0.0.0");
          
        // set up the reply and context for fsync
        frep reply;
        ClientContext context;
        
        // call updateFsync
        Status status = cdn_stub_->updateSync(&context, request, &reply);
          
        // get the count of fs
        int fscount = std::stoi(reply.fscount());
        if (fscount > 0){
            for (int i = 0; i < fscount; i++){
                
                fs tempfs(reply.id(i), reply.ip(i),reply.pno(i));
                
                bool contains = false;
                for (const fs& cur : fsyncs){
                    if (cur.id == tempfs.id) contains = true;
                }
                if (!contains){
                    fsyncs.push_back(tempfs);
                }
            }
        }
        
        sleep(3);
    }
}

void checkUpdates(){


    // may fail
    std::time_t minTime = time(0);
    std::time_t lastLogin = time(0);
    std::time_t lastFollower = time(0);
    std::map<string, time_t> lastTimeline;
    
    std::string lastLoginUpdate = "";
    std::string lastFollowerUpdate = "";
    std::map<string, string> lastTimelineUpdate;
    
    while(true){

        struct stat attr;
        
        // new logins
        string loginPath = "slave_" + id + "/loginLog.txt";
        stat(loginPath.c_str(), &attr);

        if (lastLoginUpdate != ctime(&attr.st_mtime)){
            lastLoginUpdate = ctime(&attr.st_mtime);
            
            // parse through new information
            std::ifstream logFile;
            logFile.open(loginPath);
            
            if (logFile.is_open()){
                string curUser;
                std::time_t maxTime = lastLogin;
                while(std::getline(logFile, curUser)){
                    
                    if (curUser.size() == 0){
                        break;
                    }
                    std::stringstream ss(curUser);
                    std::time_t curTime;
                    std::string user;
                    
                    ss >> curTime >> user;

                    if (curTime > lastLogin){
                        maxTime = std::max(maxTime, curTime);
                    }
                    // inform respective synchronizer about user

                    for (const fs& i : fsyncs){
                        
                        fuser request;
                        request.add_id(user);
                        request.set_t("LOGIN");
                        ClientContext c;
                        Reply rep;
                        

                        std::string login_info = i.ip + ":" + i.pno;
                        std::unique_ptr<SNSService::Stub> cur_stub_ = std::unique_ptr<SNSService::Stub>
                                        (SNSService::NewStub(
                                        grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
                    
                        
                        cur_stub_->sendUser(&c, request, &rep);

                    }
                    
                        
    
                    
                }
            
                lastLogin = maxTime;
            }
            
            logFile.close();
        }
        
        // changes to following
        string followerPath = "slave_" + id + "/followerInfo.txt";
        
        stat(followerPath.c_str(), &attr);

        if (lastFollowerUpdate != ctime(&attr.st_mtime)){
            lastFollowerUpdate = ctime(&attr.st_mtime);
            
            // parse through new information
            std::ifstream logFile;
            logFile.open(followerPath);
            
            if (logFile.is_open()){
                string curUser;
                std::time_t maxTime = lastFollower;
                while(std::getline(logFile, curUser)){
                    
                    if (curUser.size() == 0){
                        break;
                    }
                    std::stringstream ss(curUser);
                    std::time_t curTime;
                    std::string follower;
                    std::string temp;
                    std::string following;
                    
                    ss >> curTime >> follower >> temp >> following;

                    if (curTime > lastFollower){
                        maxTime = std::max(maxTime, curTime);
                        
                        // inform respective synchronizer about user
                        for (const fs& i : fsyncs){
                            
                                // TODO
                                //if (i.id == std::stoi(following) % 3 + 1){
                                
                                fuser request;
                                request.add_id(follower);
                                request.add_id(following);
                                request.set_t("FOLLOW");
                                ClientContext c;
                                Reply rep;
                                
                                std::string login_info = i.ip + ":" + i.pno;
                                std::unique_ptr<SNSService::Stub> cur_stub_ = std::unique_ptr<SNSService::Stub>
                                                (SNSService::NewStub(
                                                grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
                            
                                
                                cur_stub_->sendUser(&c, request, &rep);

                            //}
                        }
                        
                    }
    
                    
                }
            
                lastFollower = maxTime;
            }
            
            logFile.close();
        }


        // iterate through timelines
        std::vector<string> timelines;
        struct dirent *entry = nullptr;
        DIR *dp = nullptr;
    
        string dir = "slave_" + id + "/";
        dp = opendir(dir.c_str());
        if (dp != nullptr) {
            while ((entry = readdir(dp))){

                if (entry->d_name[0] == 't'){
                    timelines.push_back(entry->d_name);
                }
            }
        }
    
        closedir(dp);
        
        // changes to timelines
        for (string userNum : timelines){
         
            string timelinePath = "slave_" + id + "/" + userNum;
        
            stat(timelinePath.c_str(), &attr);
            
            if (lastTimeline.find(userNum) == lastTimeline.end()) lastTimeline.insert({userNum, minTime});
            if (lastTimelineUpdate.find(userNum) == lastTimelineUpdate.end()) lastTimelineUpdate.insert({userNum, ""});
            
            if (lastTimelineUpdate[userNum] != ctime(&attr.st_mtime)){
                lastTimelineUpdate[userNum] = ctime(&attr.st_mtime);
                
                // parse through new information
                std::ifstream logFile;
                logFile.open(timelinePath);
                
                if (logFile.is_open()){
                    string curUser;
                    std::time_t maxTime = lastTimeline[userNum];
                    while(std::getline(logFile, curUser)){
                        
                        if (curUser.size() == 0){
                            break;
                        }
                        std::stringstream ss(curUser);
                        std::time_t curTime;
                        std::string user;
                        std::string temp;
                        std::string post = "";

                        
                        ss >> curTime >> user >> temp;
                        
                        std::string restPost;
                        while(ss >> restPost){
                            post += restPost + " ";
                        }
                        post.pop_back();
    
                        if (curTime > lastTimeline[userNum] && ("t" + user + ".txt") == userNum){
                            maxTime = std::max(maxTime, curTime);
                            
                            // inform respective synchronizer about user
                            for (const fs& i : fsyncs){
                                
                                    // TODO
                                    //if (i.id == std::stoi(following) % 3 + 1){
                                    
                                    fuser request;
                                    request.add_id(user);
                                    request.set_post(post);
                                    request.set_t("TIMELINE");
                                    ClientContext c;
                                    Reply rep;
                                    
                                    std::string login_info = i.ip + ":" + i.pno;
                                    std::unique_ptr<SNSService::Stub> cur_stub_ = std::unique_ptr<SNSService::Stub>
                                                    (SNSService::NewStub(
                                                    grpc::CreateChannel(login_info, grpc::InsecureChannelCredentials())));
                                
                                    
                                    cur_stub_->sendUser(&c, request, &rep);
    
                                //}
                            }
                            
                        }
        
                        
                    }

                    lastTimeline[userNum] = maxTime;
                }
                
                logFile.close();
            }
       
        }
        
        sleep(30);
    }
}

void RunFSync(std::string port_no) {
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
  std::string cip = "0.0.0.0";
  std::string cpno = "8080";
  
  
  int opt = 0;
  while ((opt = getopt(argc, argv, "h:c:p:i:")) != -1){
    switch(opt) {
      case 'h':
          cip = optarg;break;
      case 'c':
          cpno = optarg;break;
      case 'p':
          port = optarg;break;
      case 'i':
          id = optarg;break;
      default:
	  std::cerr << "Invalid Command Line Argument\n";
    }
  }
  
    std::string persPath = "fs_master_" + id;
  if (mkdir(persPath.c_str(), 0777) == -1){
    
    if( errno == EEXIST ) {
        // alredy exists
        std::ofstream ofs;
        ofs.open(persPath + "/update.txt", std::ofstream::out | std::ofstream::trunc);
        ofs.close();
    }
  }
    persPath = "fs_slave_" + id;
  if (mkdir(persPath.c_str(), 0777) == -1){
    
    if( errno == EEXIST ) {
        // alredy exists
        std::ofstream ofs;
        ofs.open(persPath + "/update.txt", std::ofstream::out | std::ofstream::trunc);
        ofs.close();
    }
  }
  
  
  std::thread uFS(updateFS, cpno, cip, port);
  std::thread check(checkUpdates); 
  
  RunFSync(port);
  
  uFS.join();
  check.join();

  return 0;
}
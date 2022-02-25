#include <iostream>
#include <string>
#include <unistd.h>
#include <grpc++/grpc++.h>
#include "client.h"
#include "sns.grpc.pb.h"
#include <sstream>
#include <google/protobuf/timestamp.pb.h>
#include <google/protobuf/duration.pb.h>
#include <google/protobuf/util/time_util.h>


#include <thread>

using google::protobuf::Timestamp;
using csce438::SNSService;
using grpc::ClientReaderWriter;


void upper(std::string& str) 
{
    std::locale loc;
    for (std::string::size_type i = 0; i < str.size(); i++)
        str[i] = toupper(str[i], loc);
}

class Client : public IClient
{
    public:
        Client(const std::string& hname,
               const std::string& uname,
               const std::string& p)
            :hostname(hname), username(uname), port(p)
            {}
            
            
        
    protected:
        virtual int connectTo();
        virtual IReply processCommand(std::string& input);
        virtual void processTimeline();
    private:
        std::string hostname;
        std::string username;
        std::string port;
        
        // You can have an instance of the client stub
        // as a member variable.
        std::unique_ptr<SNSService::Stub> stub_;
        
        //clients timeline stream and context for the stream
        std::shared_ptr<ClientReaderWriter<csce438::Message, csce438::Message> > timelineStream;
        grpc::ClientContext stream_context;
        
};

int main(int argc, char** argv) {

    std::string hostname = "localhost";
    std::string username = "default";
    std::string port = "3010";
    int opt = 0;
    while ((opt = getopt(argc, argv, "h:u:p:")) != -1){
        switch(opt) {
            case 'h':
                hostname = optarg;break;
            case 'u':
                username = optarg;break;
            case 'p':
                port = optarg;break;
            default:
                std::cerr << "Invalid Command Line Argument\n";
        }
    }

    Client myc(hostname, username, port);
    // You MUST invoke "run_client" function to start business logic
    myc.run_client();

    return 0;
}

int Client::connectTo()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to create a stub so that
    // you call service methods in the processCommand/porcessTimeline
    // functions. That is, the stub should be accessible when you want
    // to call any service methods in those functions.
    // I recommend you to have the stub as
    // a member variable in your own Client class.
    // Please refer to gRpc tutorial how to create a stub.
	// ------------------------------------------------------------
	
	//syntax for creating channel I think
	std::string host_and_port = this->hostname + ":" + this->port;
	
	//create stub
	try{
	    stub_ = SNSService::NewStub(grpc::CreateChannel(host_and_port, grpc::InsecureChannelCredentials()));
	}catch (std::exception& e){
	    std::cerr << "Error creating stub";
	    return -1;
	}
	
    
    //login the user
    grpc::ClientContext context;
    csce438::Request req;
    csce438::Reply rep;
    
    //send login to server and see if already logged in
    req.set_username(this->username);
    grpc::Status status = stub_->Login(&context, req, &rep);
    
    if (rep.msg() == "ALREADY"){
        std::cout << "you have already joined\n";
    }
    
    return 1; // return 1 if success, otherwise return -1
}

IReply Client::processCommand(std::string& input)
{
    /*
	// ------------------------------------------------------------
	// GUIDE 1:
	// In this function, you are supposed to parse the given input
    // command and create your own message so that you call an 
    // appropriate service method. The input command will be one
    // of the followings:
	//
	// FOLLOW <username>
	// UNFOLLOW <username>
	// LIST
    // TIMELINE
	//
	// ------------------------------------------------------------
	
    // ------------------------------------------------------------
	// GUIDE 2:
	// Then, you should create a variable of IReply structure
	// provided by the client.h and initialize it according to
	// the result. Finally you can finish this function by returning
    // the IReply.
	// ------------------------------------------------------------
    
	// ------------------------------------------------------------
    // HINT: How to set the IReply?
    // Suppose you have "Follow" service method for FOLLOW command,
    // IReply can be set as follow:
    // 
    //     // some codes for creating/initializing parameters for
    //     // service method
    //     IReply ire;
    //     grpc::Status status = stub_->Follow(&context, /some parameters /);
    //     ire.grpc_status = status;
    //     if (status.ok()) {
    //         ire.comm_status = SUCCESS;
    //     } else {
    //         ire.comm_status = FAILURE_NOT_EXISTS;
    //     }
    //      
    //      return ire;
    // 
    // IMPORTANT: 
    // For the command "LIST", you should set both "all_users" and 
    // "following_users" member variable of IReply.
    // ------------------------------------------------------------
    */

    IReply ire;
    grpc::ClientContext context;
    csce438::Request req;
    csce438::Reply rep;
    
    //stream to check what the command is
    std::stringstream ss(input);
    
    std::string commType;
    ss >> commType;
    
    upper(commType);
    
    // Follow
    if (commType == "FOLLOW"){
        
        //format rpc call
        req.set_username(username);
        std::string followUser;
        ss >> followUser;
        req.add_arguments(followUser);
        
        //call rpc and interpret result
        grpc::Status status = stub_->Follow(&context, req, &rep);
        ire.grpc_status = status;
        if (status.ok()){
            if (rep.msg() == "NOUSER")
                ire.comm_status = FAILURE_INVALID_USERNAME;
            else if (rep.msg() == "ALREADY")
                ire.comm_status = FAILURE_ALREADY_EXISTS;
            else
                ire.comm_status = SUCCESS;
            
        }
         
        return ire;
        
        
    }
    //unfollow
    else if(commType == "UNFOLLOW"){
        
        //format rpc call
        req.set_username(username);
        std::string unfollowUser;
        ss >> unfollowUser;
        req.add_arguments(unfollowUser);
        
        //call rpc and interpret result
        grpc::Status status = stub_->UnFollow(&context, req, &rep);
        ire.grpc_status = status;
        if (status.ok()){
            if (rep.msg() == "NOUSER")
                ire.comm_status = FAILURE_INVALID_USERNAME;
            else if (rep.msg() == "ALREADY")
                ire.comm_status = FAILURE_NOT_EXISTS;
            else 
                ire.comm_status = SUCCESS;
            
        }
        return ire;
    }
    //list
    else if(commType == "LIST"){
        
        //format rpc call
        req.set_username(username);
        
        //call rpc and interpret result
        grpc::Status status = stub_->List(&context, req, &rep);
        ire.grpc_status = status;
        if (status.ok()){
            ire.comm_status = SUCCESS;
            for (std::string all : rep.all_users()) ire.all_users.push_back(all);
            for (std::string following : rep.following_users()) ire.following_users.push_back(following);
        }else {
            ire.comm_status = FAILURE_NOT_EXISTS;
        }
         
        return ire;
    }
    //timeline
    else if(commType == "TIMELINE"){
        
        //set up stream for read write to server
        std::shared_ptr<ClientReaderWriter<csce438::Message, csce438::Message> > stream(
            stub_->Timeline(&stream_context));
            
        //copy to class stream
        timelineStream.swap(stream);
        
        ire.grpc_status = grpc::Status::OK;
        ire.comm_status = SUCCESS;
        
        return ire;
       
    }
    //invalid input
    
    
    
    ire.comm_status = FAILURE_UNKNOWN;
    return ire;
}


void Client::processTimeline()
{
	// ------------------------------------------------------------
    // In this function, you are supposed to get into timeline mode.
    // You may need to call a service method to communicate with
    // the server. Use getPostMessage/displayPostMessage functions
    // for both getting and displaying messages in timeline mode.
    // You should use them as you did in hw1.
	// ------------------------------------------------------------

    // ------------------------------------------------------------
    // IMPORTANT NOTICE:
    //
    // Once a user enter to timeline mode , there is no way
    // to command mode. You don't have to worry about this situation,
    // and you can terminate the client program by pressing
    // CTRL-C (SIGINT)
	// ------------------------------------------------------------
	
	//send to server a timeline setup
	csce438::Message startup;
	startup.set_username(username);
	startup.set_msg("adduser");
	timelineStream->Write(startup);
	
	//thread for posting
	std::thread p([this](){
	    csce438::Message msg;
        msg.set_username(this->username);
        Timestamp ts;
    
        while(1){
            //wait for a message
            std::string post;
            post = getPostMessage();
            
            //construct Message to send to server
            msg.set_msg(post);
            //google::protobuf::util::TimeUtil::Timestamp ts;

            

            //write to stream
            this->timelineStream->Write(msg);
            //timelineStream->WritesDone();
            
        }
	});
	
	//code for diplaying post
	csce438::Message server_msg;
    while(1){
        //read in from server
        timelineStream->Read(&server_msg);
        //display message
        std::string poster = server_msg.username();
        std::string post = server_msg.msg();
        std::time_t post_time = google::protobuf::util::TimeUtil::TimestampToTimeT(server_msg.timestamp());
        displayPostMessage(poster, post, post_time);
        
        
        
    }
    p.join();
    timelineStream->Finish();
	

	
}


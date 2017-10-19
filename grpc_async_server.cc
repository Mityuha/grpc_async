
#ifndef METEO_GRPC_SERVER_H
#define METEO_GRPC_SERVER_H

#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <iostream>
#include <cmath>
#include "assert.h"

#include <grpc++/grpc++.h>

#include "helloworld.grpc.pb.h"

using grpc::Server;
using grpc::ServerAsyncResponseWriter;
using grpc::ServerAsyncWriter;
using grpc::ServerAsyncReader;
using grpc::ServerAsyncReaderWriter;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::ServerCompletionQueue;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;


class CommonCallData
{
	public:
    // The means of communication with the gRPC runtime for an asynchronous
    // server.
    Greeter::AsyncService* service_;
    // The producer-consumer queue where for asynchronous server notifications.
    ServerCompletionQueue* cq_;
    // Context for the rpc, allowing to tweak aspects of it such as the use
    // of compression, authentication, as well as to send metadata back to the
    // client.
    ServerContext ctx_;
    // What we get from the client.
    HelloRequest request_;
    // What we send back to the client.
    HelloReply reply_;
	// Let's implement a tiny state machine with the following states.
    enum CallStatus { CREATE, PROCESS, FINISH };
    CallStatus status_;  // The current serving state.
    std::string prefix;

	public:
	explicit CommonCallData(Greeter::AsyncService* service, ServerCompletionQueue* cq):
						service_(service), cq_(cq),status_(CREATE),prefix("Hello ")
	{}

	virtual ~CommonCallData()
	{
//		std::cout << "CommonCallData destructor" << std::endl;
	}

	virtual void Proceed(bool = true) = 0;
};

class CallData: public CommonCallData
{
    ServerAsyncResponseWriter<HelloReply> responder_;
	public:
	CallData(Greeter::AsyncService* service, ServerCompletionQueue* cq):
		CommonCallData(service, cq), responder_(&ctx_){Proceed();}

	virtual void Proceed(bool = true) override
	{
		if (status_ == CREATE)
		{
			std::cout << "[Proceed11]: New responder for 1-1 mode" << std::endl;
	        status_ = PROCESS;
	        service_->RequestSayHello(&ctx_, &request_, &responder_, cq_, cq_, this);
      	}
		else if (status_ == PROCESS)
		{
	        new CallData(service_, cq_);
			std::cout << "[Proceed11]: request message = " << request_.name() << std::endl;
       		reply_.set_message(prefix + request_.name());
	        status_ = FINISH;
    	    responder_.Finish(reply_, Status::OK, this);
      	}
		else
		{
        	GPR_ASSERT(status_ == FINISH);
			std::cout << "[Proceed11]: Good Bye" << std::endl;
    	    delete this;
		}
	}
};


class CallData1M: public CommonCallData
{
    ServerAsyncWriter<HelloReply> responder_;
	unsigned mcounter;
	bool new_responder_created;
public:
	CallData1M(Greeter::AsyncService* service, ServerCompletionQueue* cq):
		CommonCallData(service, cq), responder_(&ctx_), mcounter(0), new_responder_created(false){ Proceed() ;}

	virtual void Proceed(bool = true) override
	{
		if(status_ == CREATE)
		{
			std::cout << "[Proceed1M]: New responder for 1-M mode" << std::endl;
			service_->RequestGladToSeeMe(&ctx_, &request_, &responder_, cq_, cq_, this);
			status_ = PROCESS ;
		}
		else if(status_ == PROCESS)
		{
			if(!new_responder_created)
			{
				new CallData1M(service_, cq_);
				new_responder_created = true ;
				std::cout << "[Proceed1M]: request message = " << request_.name() << std::endl;
			}
			std::vector<std::string> greeting = {std::string(prefix + request_.name() + "!"),
												"I'm very glad to see you!",
												"Haven't seen you for thousand years.",
												"I'm server now. Call me later."};

			std::cout << "[Proceed1M]: mcounter = " << mcounter << std::endl;
			if(ctx_.IsCancelled() || mcounter >= greeting.size())
			{
				std::cout << "[Proceed1M]: Trying finish" << std::endl;
				status_ = FINISH;
				responder_.Finish(Status(), (void*)this);
			}
			else
			{
				reply_.set_message(greeting.at(mcounter));
				std::cout << "[Proceed1M]: Writing" << std::endl;
				responder_.Write(reply_, (void*)this);
				++mcounter;
			}
		}
		else if(status_ == FINISH)
		{
			std::cout << "[Proceed1M]: Good Bye" << std::endl;
			delete this;
		}
	}
};


class CallDataM1: public CommonCallData
{
    ServerAsyncReader<HelloReply, HelloRequest> responder_;
	bool new_responder_created;
	public:
	CallDataM1(Greeter::AsyncService* service, ServerCompletionQueue* cq):
		CommonCallData(service, cq), responder_(&ctx_), new_responder_created(false){Proceed();}

	virtual void Proceed(bool ok = true) override
	{
		if(status_ == CREATE)
		{
			std::cout << "[ProceedM1]: New responder for M-1 mode" << std::endl;
			status_ = PROCESS ;
			service_->RequestGladToSeeYou(&ctx_, &responder_, cq_, cq_, this);
		}
		else if(status_ == PROCESS)
		{
			if(!new_responder_created)
			{
				new CallDataM1(service_, cq_);
				new_responder_created = true ;
			}
			//It's time to send reply
			if(!ok)
			{
				std::string greeting("Hello, Client!");
				reply_.set_message(greeting);
				std::cout << "[ProceedM1]: Sending reply" << std::endl;
				status_ = FINISH;
				responder_.Finish(reply_, Status(), (void*)this);
			}
			else
			{
				responder_.Read(&request_, (void*)this);
				if(!request_.name().empty())
					std::cout << "[ProceedM1]: request message =" << request_.name() << std::endl;
			}
		}
		else
		{
			std::cout << "[ProceedM1]: Good Bye" << std::endl;
			delete this;
		}
	}
};


class CallDataMM: public CommonCallData
{
    ServerAsyncReaderWriter<HelloReply, HelloRequest> responder_;
	unsigned mcounter;
	bool writing_mode_;
	bool new_responder_created;
	public:
	CallDataMM(Greeter::AsyncService* service, ServerCompletionQueue* cq):
		CommonCallData(service, cq), responder_(&ctx_), mcounter(0), writing_mode_(false), new_responder_created(false){Proceed();}

	virtual void Proceed(bool ok = true) override
	{
		if(status_ == CREATE)
		{
			std::cout << "[ProceedMM]: New responder for M-M mode" << std::endl;
			status_ = PROCESS ;
			service_->RequestBothGladToSee(&ctx_, &responder_, cq_, cq_, this);
		}
		else if(status_ == PROCESS)
		{
			if(!new_responder_created)
			{
				new CallDataMM(service_, cq_);
				new_responder_created = true ;
			}
			if(!writing_mode_)//reading mode
			{
				if(!ok)
				{
					writing_mode_ = true;
					ok = true;
					std::cout << "[ProceedMM]: changing state to writing" << std::endl;
				}
				else
				{
					responder_.Read(&request_, (void*)this);
					if(!request_.name().empty())
						std::cout << "[ProceedMM]: request message =" << request_.name() << std::endl;
				}
			}
			if(writing_mode_)//writing mode
			{
				std::vector<std::string> greeting = {std::string(prefix + "client" "!"),
												"I'm very glad to see you!",
												"Haven't seen you for thousand years.",
												"How are you?",
												"I'm server now. Call me later."};
				if(!ok || mcounter >= greeting.size() || ctx_.IsCancelled())
				{
					std::cout << "[ProceedMM]: Trying finish" << std::endl;
					status_ = FINISH;
					responder_.Finish(Status(), (void*)this);
				}
				else
				{
					reply_.set_message(greeting.at(mcounter));
					responder_.Write(reply_, (void*)this);
					++mcounter;
				}
			}

		}
		else
		{
			std::cout << "[ProceedMM]: Good Bye" << std::endl;
			delete this;
		}
	}
};



class ServerImpl
{
public:
	~ServerImpl()
	{
	    server_->Shutdown();
   		 // Always shutdown the completion queue after the server.
   		cq_->Shutdown();
  	}

    void Run()
    {
        std::string server_address("0.0.0.0:50051");

        ServerBuilder builder;
        // Listen on the given address without any authentication mechanism.
        builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
        // Register "service_" as the instance through which we'll communicate with
        // clients. In this case it corresponds to an *asynchronous* service.
        builder.RegisterService(&service_);
        // Get hold of the completion queue used for the asynchronous communication
        // with the gRPC runtime.
        cq_ = builder.AddCompletionQueue();
        // Finally assemble the server.
        server_ = builder.BuildAndStart();
        std::cout << "Server listening on " << server_address << std::endl;

        // Proceed to the server's main loop.
        // Spawn a new CallData instance to serve new clients.
        new CallData(&service_, cq_.get());
        new CallData1M(&service_, cq_.get());
        new CallDataM1(&service_, cq_.get());
        new CallDataMM(&service_, cq_.get());

        void* tag;  // uniquely identifies a request.
        bool ok;
        while(true)
        {
            GPR_ASSERT(cq_->Next(&tag, &ok));
            CommonCallData* calldata = static_cast<CommonCallData*>(tag);
            calldata->Proceed(ok);
        }
    }

private:
	std::unique_ptr<ServerCompletionQueue> cq_;
	Greeter::AsyncService service_;
	std::unique_ptr<Server> server_;
};


int main(int argc, char* argv[])
{
    ServerImpl server;
    server.Run();
}


#endif

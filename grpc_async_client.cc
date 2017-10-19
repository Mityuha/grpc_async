
#ifndef METEO_GRPC_CLIENT_H
#define METEO_GRPC_CLIENT_H

#include <functional>
#include <stdexcept>
#include <boost/utility.hpp>
#include <boost/type_traits.hpp>
#include <memory>
#include <iostream>
#include <cmath>
#include "assert.h"

#include <grpc++/grpc++.h>
#include <thread>

#include "helloworld.grpc.pb.h"


using grpc::Channel;
using grpc::ClientAsyncResponseReader;
using grpc::ClientContext;
using grpc::ClientAsyncReader;
using grpc::ClientAsyncWriter;
using grpc::ClientAsyncReaderWriter;
using grpc::CompletionQueue;
using grpc::Status;
using helloworld::HelloRequest;
using helloworld::HelloReply;
using helloworld::Greeter;


class AbstractAsyncClientCall
{
public:
	enum CallStatus { PROCESS, FINISH, DESTROY };

	explicit AbstractAsyncClientCall():callStatus(PROCESS){}
	virtual ~AbstractAsyncClientCall(){}
	HelloReply reply;
	ClientContext context;
	Status status;
	CallStatus callStatus ;
	void printReply(const char* from)
	{
		if(!reply.message().empty())
			std::cout << "[" << from << "]: reply message = " << reply.message() << std::endl;
		else
			std::cout << "[" << from << "]: reply message empty" << std::endl;

	}
	virtual void Proceed(bool = true) = 0;
};

class AsyncClientCall: public AbstractAsyncClientCall
{
	std::unique_ptr< ClientAsyncResponseReader<HelloReply> > responder;
public:
	AsyncClientCall(const HelloRequest& request, CompletionQueue& cq_, std::unique_ptr<Greeter::Stub>& stub_):AbstractAsyncClientCall()
	{
		std::cout << "[Proceed11]: new client 1-1" << std::endl;
	    responder = stub_->AsyncSayHello(&context, request, &cq_);
	    responder->Finish(&reply, &status, (void*)this);
		callStatus = PROCESS ;
	}
	virtual void Proceed(bool ok = true) override
	{
		if(callStatus == PROCESS)
		{
        // Verify that the request was completed successfully. Note that "ok"
        // corresponds solely to the request for updates introduced by Finish().
	        GPR_ASSERT(ok);
			if(status.ok())
				printReply("Proceed11");
			std::cout << "[Proceed11]: Good Bye" << std::endl;
			delete this;
		}
	}
};

class AsyncClientCall1M : public AbstractAsyncClientCall
{
	std::unique_ptr< ClientAsyncReader<HelloReply> > responder;
public:
	AsyncClientCall1M(const HelloRequest& request, CompletionQueue& cq_, std::unique_ptr<Greeter::Stub>& stub_):AbstractAsyncClientCall()
	{
		std::cout << "[Proceed1M]: new client 1-M" << std::endl;
	    responder = stub_->AsyncGladToSeeMe(&context, request, &cq_, (void*)this);
		callStatus = PROCESS ;
	}
	virtual void Proceed(bool ok = true) override
	{
		if(callStatus == PROCESS)
		{
			if(!ok)
			{
				responder->Finish(&status, (void*)this);
				callStatus = FINISH;
				return ;
			}
			responder->Read(&reply, (void*)this);
			printReply("Proceed1M");
		}
		else if(callStatus == FINISH)
		{
			std::cout << "[Proceed1M]: Good Bye" << std::endl;
			delete this;
		}
		return ;
	}
};

class AsyncClientCallM1 : public AbstractAsyncClientCall
{
	std::unique_ptr< ClientAsyncWriter<HelloRequest> > responder;
	unsigned mcounter;
	bool writing_mode_;
public:
	AsyncClientCallM1(CompletionQueue& cq_, std::unique_ptr<Greeter::Stub>& stub_):
		AbstractAsyncClientCall(), mcounter(0),writing_mode_(true)
	{
		std::cout << "[ProceedM1]: new client M-1" << std::endl;
	    responder = stub_->AsyncGladToSeeYou(&context, &reply, &cq_, (void*)this);
		callStatus = PROCESS ;
	}
	virtual void Proceed(bool ok = true) override
	{
		if(callStatus == PROCESS)
		{
			if(writing_mode_)
			{
				static std::vector<std::string> greeting = {"Hello, server!",
    	                                    	"Glad to see you!",
        	                                	"Haven't seen you for thousand years!",
            	                            	"I'm client now. Call me later."};
				//std::cout << "[ProceedM1]: mcounter = " << mcounter << std::endl;
				if(mcounter < greeting.size())
    			{
        			HelloRequest request;
        			request.set_name(greeting.at(mcounter));
					responder->Write(request, (void*)this);
					++mcounter ;
    			}
    			else
    			{
           			responder->WritesDone((void*)this);
					std::cout << "[ProceedM1]: changing state to reading" << std::endl;
					writing_mode_ = false;
					return;
    			}
			}
			else//reading mode
			{
				std::cout << "[ProceedM1]: trying finish" << std::endl;
    			responder->Finish(&status, (void*)this);
				callStatus = FINISH ;
			}
		}
		else if(callStatus == FINISH)
		{
			assert(!reply.message().empty());
			printReply("ProceedM1");
			std::cout << "[ProceedM1]: Good Bye" << std::endl;
			delete this;
		}
		return ;
	}
};

class AsyncClientCallMM : public AbstractAsyncClientCall
{
	std::unique_ptr< ClientAsyncReaderWriter<HelloRequest,HelloReply> > responder;
	unsigned mcounter;
	bool writing_mode_;
public:
	AsyncClientCallMM(CompletionQueue& cq_, std::unique_ptr<Greeter::Stub>& stub_):
	AbstractAsyncClientCall(), mcounter(0), writing_mode_(true)
	{
		std::cout << "[ProceedMM]: new client M-M" << std::endl;
    	responder = stub_->AsyncBothGladToSee(&context, &cq_, (void*)this);
		callStatus = PROCESS ;
	}
	virtual void Proceed(bool ok = true) override
	{
		if(callStatus == PROCESS)
		{

			if(writing_mode_)
			{
				static std::vector<std::string> greeting = {"Hello, server!",
    	                                    	"Glad to see you!",
        	                                	"Haven't seen you for thousand years!",
            	                            	"I'm client now. Call me later."};
				//std::cout << "[ProceedMM]: mcounter = " << mcounter << std::endl;
    			if(mcounter < greeting.size())
    			{
        			HelloRequest request;
        			request.set_name(greeting.at(mcounter));
           			responder->Write(request, (void*)this);
        			++mcounter;
    			}
    			else
    			{
            		responder->WritesDone((void*)this);
					std::cout << "[ProceedMM]: changing state to reading" << std::endl;
        			writing_mode_ = false;
    			}
				return ;
			}
			else //reading mode
			{
				if(!ok)
				{
					std::cout << "[ProceedMM]: trying finish" << std::endl;
					callStatus = FINISH;
					responder->Finish(&status, (void*)this);
					return;
				}
				responder->Read(&reply, (void*)this);
				printReply("ProceedMM");
			}
		    return;
		}
		else if(callStatus == FINISH)
		{
			std::cout << "[ProceedMM]: Good Bye" << std::endl;
			delete this;
		}

	}
};




class GreeterClient
{
public:
    explicit GreeterClient(std::shared_ptr<Channel> channel)
            :stub_(Greeter::NewStub(channel))
	{}

	// Assembles the client's payload and sends it to the server.
    void SayHello(const std::string& user)
	{
	    HelloRequest request;
    	request.set_name(user);
    	new AsyncClientCall(request, cq_, stub_);
	}


	void GladToSeeMe(const std::string& user)
	{
		HelloRequest request;
	    request.set_name(user);
	    new AsyncClientCall1M(request, cq_, stub_);
	}


	void GladToSeeYou()
	{
	    new AsyncClientCallM1(cq_, stub_);
	}


	void BothGladToSee()
	{
    	new AsyncClientCallMM(cq_, stub_);
	}

	void AsyncCompleteRpc()
	{
		void* got_tag;
    	bool ok = false;
		while(cq_.Next(&got_tag, &ok))
		{
        	AbstractAsyncClientCall* call = static_cast<AbstractAsyncClientCall*>(got_tag);
			call->Proceed(ok);
    	}
		std::cout << "Completion queue is shutting down." << std::endl;
	}

private:
    // Out of the passed in Channel comes the stub, stored here, our view of the
    // server's exposed services.
    std::unique_ptr<Greeter::Stub> stub_;

    // The producer-consumer queue we use to communicate asynchronously with the
    // gRPC runtime.
    CompletionQueue cq_;
};


int main(int argc, char* argv[])
{
	GreeterClient greeter(grpc::CreateChannel("localhost:50051", grpc::InsecureChannelCredentials()));
	std::thread thread_ = std::thread(&GreeterClient::AsyncCompleteRpc, &greeter);
	greeter.SayHello("world");
	greeter.GladToSeeMe("client");
	greeter.GladToSeeYou();
	greeter.BothGladToSee();
	thread_.join();

}


#endif

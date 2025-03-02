#include <iostream>
#include <memory>
#include <string>
#include <chrono>
#include <thread>
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

using grpc::Channel;
using grpc::ClientContext;
using grpc::Status;
using microservice::ServiceTwo;
using microservice::Request;
using microservice::Response;

class ServiceOneClient {
public:
  ServiceOneClient(std::shared_ptr<Channel> channel)
      : stub_(ServiceTwo::NewStub(channel)) {}

  std::string SendRequest(const std::string& data) {
    Request request;
    request.set_data(data);

        Response response;
        ClientContext context;

        Status status = stub_->ProcessRequest(&context, request, &response);

    if (status.ok()) {
      return response.result();
    } else {
      return "RPC failed: " + status.error_message();
    }
  }

private:
  std::unique_ptr<ServiceTwo::Stub> stub_;
};

int main() {
  std::string target_address("service2:50051");
  ServiceOneClient client(
    grpc::CreateChannel(target_address, grpc::InsecureChannelCredentials()));

  // Run indefinitely
  while (true) {
      std::string response = client.SendRequest("Hello from Service 1");
      std::cout << "Response received: " << response << std::endl;
      
      // Sleep for 1 second between requests
      std::this_thread::sleep_for(std::chrono::seconds(1));
  }

  return 0;
} 
#include <iostream>
#include <memory>
#include <string>
#include <grpcpp/grpcpp.h>
#include "service.grpc.pb.h"

using grpc::Server;
using grpc::ServerBuilder;
using grpc::ServerContext;
using grpc::Status;
using microservice::ServiceTwo;
using microservice::Request;
using microservice::Response;

class ServiceTwoImpl final : public ServiceTwo::Service {
  Status ProcessRequest(ServerContext* context, const Request* request,
                       Response* response) override {
    std::string prefix("Processed by Service 2: ");
    response->set_result(prefix + request->data());
    return Status::OK;
  }
};

int main() {
  std::string server_address("0.0.0.0:50051");
  ServiceTwoImpl service;

  ServerBuilder builder;
  builder.AddListeningPort(server_address, grpc::InsecureServerCredentials());
  builder.RegisterService(&service);

  std::unique_ptr<Server> server(builder.BuildAndStart());
  std::cout << "Service 2 listening on " << server_address << std::endl;
  server->Wait();

  return 0;
} 
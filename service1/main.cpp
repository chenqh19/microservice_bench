#include <iostream>
#include <string>
#include <chrono>
#include <thread>
#include "service.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

class ServiceOneClient {
public:
    ServiceOneClient(const std::string& host, int port)
        : host_(host), port_(port) {}

    std::string SendRequest(const std::string& data) {
        httplib::Client cli(host_, port_);
        
        // Create and serialize request
        microservice::Request request;
        request.set_data(data);
        std::string serialized_request = microservice::utils::serialize_message(request);

        // Send POST request
        auto result = cli.Post("/process", serialized_request, "application/x-protobuf");
        
        if (result && result->status == 200) {
            microservice::Response response;
            if (microservice::utils::deserialize_message(result->body, response)) {
                return response.result();
            }
            return "Failed to parse response";
        }
        return "HTTP request failed";
    }

private:
    std::string host_;
    int port_;
};

int main() {
    ServiceOneClient client("service2", 50051);

    while (true) {
        std::string response = client.SendRequest("Hello from Service 1");
        std::cout << "Response received: " << response << std::endl;
        
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    return 0;
} 
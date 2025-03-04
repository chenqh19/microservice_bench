#include <iostream>
#include <string>
#include "service.pb.h"
#include "serialization_utils.h"
#include <httplib.h>

int main() {
    httplib::Server svr;

    svr.Post("/process", [](const httplib::Request& req, httplib::Response& res) {
        microservice::Request request;
        if (microservice::utils::deserialize_message(req.body, request)) {
            microservice::Response response;
            std::string prefix("Processed by Service 2: ");
            response.set_result(prefix + request.data());
            
            std::string serialized_response = microservice::utils::serialize_message(response);
            res.set_content(serialized_response, "application/x-protobuf");
        } else {
            res.status = 400;
            res.set_content("Failed to parse request", "text/plain");
        }
    });

    std::cout << "Service 2 listening on 0.0.0.0:50051" << std::endl;
    svr.listen("0.0.0.0", 50051);

    return 0;
} 
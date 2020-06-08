#include <iostream>
#include <vector>
#include <algorithm>
#include <iterator>
#include <memory>
#include <fstream>
#include <codecvt>
#include <memory>
#include <chrono>
#include <locale>

#include <Poco/String.h>
#include <Poco/Format.h>
#include <Poco/SharedPtr.h>

#include <Poco/JSON/Object.h>
#include <Poco/JSON/Stringifier.h>
#include <Poco/JSON/Parser.h>
#include <Poco/StreamCopier.h>

#include <Poco/Net/ServerSocket.h>
#include <Poco/Net/HTTPServer.h>
#include <Poco/Net/HTTPRequestHandler.h>
#include <Poco/Net/HTTPRequestHandlerFactory.h>
#include <Poco/Net/HTTPResponse.h>
#include <Poco/Net/HTTPServerRequest.h>
#include <Poco/Net/HTTPServerResponse.h>

#include "bk_tree.hpp"

using namespace Poco::JSON;
using namespace Poco::Net;
using namespace Poco::Util;

class CorrectorHTTPRequestsHandler : public HTTPRequestHandler {
public:
    explicit CorrectorHTTPRequestsHandler(std::shared_ptr<BKTree> dictionary)
        : HTTPRequestHandler(), dictionary_(std::move(dictionary)), parser_() {
    }

    void handleRequest(HTTPServerRequest& http_request, HTTPServerResponse& http_response) override {
        std::istream& request_stream = http_request.stream();
        Array::Ptr requests_array = parser_.parse(request_stream).extract<Array::Ptr>();
        Array::Ptr response_array = Poco::SharedPtr(new Array());
        for (size_t index = 0; index < requests_array->size(); ++index) {
            auto start_time = std::chrono::high_resolution_clock::now();

            auto request = requests_array->getObject(index);
            auto request_word_bytes = request->getValue<std::string>("candidate");
            auto request_tolerance = request->getValue<uint32_t>("max_tolerance");
            std::wstring request_word = converter_.from_bytes(request_word_bytes);

            auto search_result = dictionary_->FindSimilar(request_word, request_tolerance);
            auto search_result_array = Array();
            for (const auto& elem: search_result) {
                auto json_object = Object();
                json_object.set("word", converter_.to_bytes(elem.result));
                json_object.set("tolerance", elem.tolerance);
                json_object.set("priority", elem.priority);
                search_result_array.add(json_object);
            }

            auto finish_time = std::chrono::high_resolution_clock::now();

            auto json_response = Object();
            json_response.set("word", converter_.to_bytes(request_word));
            json_response.set("tolerance", request_tolerance);
            json_response.set("results", search_result_array);
            json_response.set("milliseconds",
                    std::chrono::duration_cast<std::chrono::milliseconds>(finish_time - start_time).count());

            response_array->set(index, json_response);
        }

        response_array->stringify(http_response.send(), 4);
        http_response.setStatus(HTTPServerResponse::HTTP_OK);
    }

private:
    std::shared_ptr<BKTree> dictionary_;
    Poco::JSON::Parser parser_;
    std::wstring_convert<std::codecvt_utf8<wchar_t>> converter_;
};


class CorrectorHandlerFactory : public HTTPRequestHandlerFactory {
public:
    explicit CorrectorHandlerFactory(std::shared_ptr<BKTree> dictionary)
        : HTTPRequestHandlerFactory(), dictionary_(std::move(dictionary)) {
    }

    HTTPRequestHandler* createRequestHandler(
            const HTTPServerRequest& request) override {

        if (request.getMethod() != HTTPRequest::HTTP_POST) {
            return nullptr;
        }

        if (request.getURI() == "/correct") {
            return new CorrectorHTTPRequestsHandler(dictionary_);
        }

        return nullptr;
    }
private:
    std::shared_ptr<BKTree> dictionary_;
};

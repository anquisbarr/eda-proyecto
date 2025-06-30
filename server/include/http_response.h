#pragma once

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    HttpResponse();
    
    void setStatus(int code, const std::string& message = "");
    void setHeader(const std::string& name, const std::string& value);
    void setBody(const std::string& body);
    void setContentType(const std::string& content_type);
    
    std::string toString() const;
    
    // Common status codes
    static const int OK = 200;
    static const int NOT_FOUND = 404;
    static const int INTERNAL_SERVER_ERROR = 500;
    static const int BAD_REQUEST = 400;

private:
    int status_code_;
    std::string status_message_;
    std::unordered_map<std::string, std::string> headers_;
    std::string body_;
    
    std::string getDefaultStatusMessage(int code) const;
}; 
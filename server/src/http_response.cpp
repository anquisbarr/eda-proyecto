#include "http_response.h"
#include <sstream>

HttpResponse::HttpResponse() : status_code_(200), status_message_("OK") {
    setHeader("Server", "C++ HTTP Server/1.0");
    setHeader("Connection", "close");
}

void HttpResponse::setStatus(int code, const std::string& message) {
    status_code_ = code;
    if (message.empty()) {
        status_message_ = getDefaultStatusMessage(code);
    } else {
        status_message_ = message;
    }
}

void HttpResponse::setHeader(const std::string& name, const std::string& value) {
    headers_[name] = value;
}

void HttpResponse::setBody(const std::string& body) {
    body_ = body;
    setHeader("Content-Length", std::to_string(body_.length()));
}

void HttpResponse::setContentType(const std::string& content_type) {
    setHeader("Content-Type", content_type);
}

std::string HttpResponse::toString() const {
    std::ostringstream response;
    
    // Status line
    response << "HTTP/1.1 " << status_code_ << " " << status_message_ << "\r\n";
    
    // Headers
    for (const auto& header : headers_) {
        response << header.first << ": " << header.second << "\r\n";
    }
    
    // Empty line between headers and body
    response << "\r\n";
    
    // Body
    response << body_;
    
    return response.str();
}

std::string HttpResponse::getDefaultStatusMessage(int code) const {
    switch (code) {
        case 200: return "OK";
        case 201: return "Created";
        case 204: return "No Content";
        case 400: return "Bad Request";
        case 401: return "Unauthorized";
        case 403: return "Forbidden";
        case 404: return "Not Found";
        case 405: return "Method Not Allowed";
        case 500: return "Internal Server Error";
        case 501: return "Not Implemented";
        case 502: return "Bad Gateway";
        case 503: return "Service Unavailable";
        default: return "Unknown";
    }
} 
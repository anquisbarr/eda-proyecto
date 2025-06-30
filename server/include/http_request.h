#pragma once

#include <string>
#include <unordered_map>

class HttpRequest {
public:
    HttpRequest() = default;
    
    void parseRequest(const std::string& raw_request);
    
    const std::string& getMethod() const { return method_; }
    const std::string& getPath() const { return path_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getBody() const { return body_; }
    
    std::string getHeader(const std::string& name) const;
    bool hasHeader(const std::string& name) const;
    
    std::string getQueryParam(const std::string& name) const;
    bool hasQueryParam(const std::string& name) const;

private:
    std::string method_;
    std::string path_;
    std::string version_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;
    std::unordered_map<std::string, std::string> query_params_;
    
    void parseRequestLine(const std::string& line);
    void parseHeaders(const std::string& headers_section);
    void parseQueryString(const std::string& query_string);
    std::string toLowerCase(const std::string& str) const;
}; 
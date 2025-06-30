#include "http_request.h"
#include <sstream>
#include <algorithm>
#include <iostream>

void HttpRequest::parseRequest(const std::string& raw_request) {
    // Let's take a simpler approach - split on double newline
    size_t body_start = raw_request.find("\r\n\r\n");
    if (body_start == std::string::npos) {
        body_start = raw_request.find("\n\n");
        if (body_start != std::string::npos) {
            body_start += 2;
        }
    } else {
        body_start += 4;
    }
    
    std::string headers_part;
    if (body_start != std::string::npos) {
        headers_part = raw_request.substr(0, body_start);
        body_ = raw_request.substr(body_start);
    } else {
        headers_part = raw_request;
        body_ = "";
    }
    
    // Parse headers part line by line
    std::istringstream headers_stream(headers_part);
    std::string line;
    
    // Parse request line
    if (std::getline(headers_stream, line)) {
        if (line.back() == '\r') line.pop_back();
        parseRequestLine(line);
    }
    
    // Parse headers
    std::string headers_section;
    while (std::getline(headers_stream, line)) {
        if (line.back() == '\r') {
            line.pop_back();
        }
        if (!line.empty()) {
            headers_section += line + "\n";
        }
    }
    parseHeaders(headers_section);
}

void HttpRequest::parseRequestLine(const std::string& line) {
    std::istringstream stream(line);
    std::string path_and_query;
    
    stream >> method_ >> path_and_query >> version_;
    
    // Parse path and query string
    size_t query_pos = path_and_query.find('?');
    if (query_pos != std::string::npos) {
        path_ = path_and_query.substr(0, query_pos);
        std::string query_string = path_and_query.substr(query_pos + 1);
        parseQueryString(query_string);
    } else {
        path_ = path_and_query;
    }
}

void HttpRequest::parseHeaders(const std::string& headers_section) {
    std::istringstream stream(headers_section);
    std::string line;
    
    while (std::getline(stream, line)) {
        size_t colon_pos = line.find(':');
        if (colon_pos != std::string::npos) {
            std::string name = line.substr(0, colon_pos);
            std::string value = line.substr(colon_pos + 1);
            
            // Trim whitespace
            name.erase(0, name.find_first_not_of(" \t"));
            name.erase(name.find_last_not_of(" \t") + 1);
            value.erase(0, value.find_first_not_of(" \t"));
            value.erase(value.find_last_not_of(" \t") + 1);
            
            headers_[toLowerCase(name)] = value;
        }
    }
}

void HttpRequest::parseQueryString(const std::string& query_string) {
    std::istringstream stream(query_string);
    std::string pair;
    
    while (std::getline(stream, pair, '&')) {
        size_t eq_pos = pair.find('=');
        if (eq_pos != std::string::npos) {
            std::string name = pair.substr(0, eq_pos);
            std::string value = pair.substr(eq_pos + 1);
            query_params_[name] = value;
        }
    }
}

std::string HttpRequest::getHeader(const std::string& name) const {
    auto it = headers_.find(toLowerCase(name));
    return (it != headers_.end()) ? it->second : "";
}

bool HttpRequest::hasHeader(const std::string& name) const {
    return headers_.find(toLowerCase(name)) != headers_.end();
}

std::string HttpRequest::getQueryParam(const std::string& name) const {
    auto it = query_params_.find(name);
    return (it != query_params_.end()) ? it->second : "";
}

bool HttpRequest::hasQueryParam(const std::string& name) const {
    return query_params_.find(name) != query_params_.end();
}

std::string HttpRequest::toLowerCase(const std::string& str) const {
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(), ::tolower);
    return result;
} 
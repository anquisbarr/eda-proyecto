#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <thread>

HttpServer::HttpServer(int port) : port_(port), server_socket_(-1), running_(false) {
}

HttpServer::~HttpServer() {
    stop();
}

void HttpServer::start() {
    // Create socket
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ < 0) {
        throw std::runtime_error("Failed to create socket");
    }
    
    // Set socket options
    int opt = 1;
    if (setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        throw std::runtime_error("Failed to set socket options");
    }
    
    // Bind socket
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port_);
    
    if (bind(server_socket_, (struct sockaddr*)&address, sizeof(address)) < 0) {
        throw std::runtime_error("Failed to bind socket to port " + std::to_string(port_));
    }
    
    // Listen for connections
    if (listen(server_socket_, 10) < 0) {
        throw std::runtime_error("Failed to listen on socket");
    }
    
    running_ = true;
    
    // Accept connections
    while (running_) {
        struct sockaddr_in client_address;
        socklen_t client_len = sizeof(client_address);
        
        int client_socket = accept(server_socket_, (struct sockaddr*)&client_address, &client_len);
        if (client_socket < 0) {
            if (running_) {
                std::cerr << "Failed to accept client connection" << std::endl;
            }
            continue;
        }
        
        // Handle client in a separate thread
        std::thread client_thread(&HttpServer::handleClient, this, client_socket);
        client_thread.detach();
    }
}

void HttpServer::stop() {
    running_ = false;
    if (server_socket_ >= 0) {
        close(server_socket_);
        server_socket_ = -1;
    }
}

void HttpServer::addRoute(const std::string& method, const std::string& path, RouteHandler handler) {
    std::string key = getRouteKey(method, path);
    routes_[key] = handler;
}

void HttpServer::setStaticDirectory(const std::string& directory) {
    static_directory_ = directory;
}

void HttpServer::handleClient(int client_socket) {
    try {
        processRequest(client_socket);
    } catch (const std::exception& e) {
        std::cerr << "Error handling client: " << e.what() << std::endl;
    }
    close(client_socket);
}

void HttpServer::processRequest(int client_socket) {
    // Read request
    char buffer[4096];
    ssize_t bytes_read = read(client_socket, buffer, sizeof(buffer) - 1);
    if (bytes_read <= 0) {
        return;
    }
    
    buffer[bytes_read] = '\0';
    std::string raw_request(buffer);
    
    // Parse request
    HttpRequest request;
    HttpResponse response;
    
    try {
        request.parseRequest(raw_request);
        
        // Check for route handler
        std::string route_key = getRouteKey(request.getMethod(), request.getPath());
        auto it = routes_.find(route_key);
        
        if (it != routes_.end()) {
            // Execute route handler
            it->second(request, response);
        } else if (request.getMethod() == "GET" && !static_directory_.empty()) {
            // Serve static file
            serveStaticFile(request.getPath(), response);
        } else {
            // 404 Not Found
            response.setStatus(HttpResponse::NOT_FOUND);
            response.setContentType("text/html");
            response.setBody("<html><body><h1>404 Not Found</h1><p>The requested resource was not found.</p></body></html>");
        }
        
    } catch (const std::exception& e) {
        // 500 Internal Server Error
        response.setStatus(HttpResponse::INTERNAL_SERVER_ERROR);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>500 Internal Server Error</h1><p>" + std::string(e.what()) + "</p></body></html>");
    }
    
    // Send response
    std::string response_str = response.toString();
    send(client_socket, response_str.c_str(), response_str.length(), 0);
}

std::string HttpServer::getRouteKey(const std::string& method, const std::string& path) {
    return method + " " + path;
}

void HttpServer::serveStaticFile(const std::string& path, HttpResponse& response) {
    std::string file_path = static_directory_;
    
    if (path == "/") {
        file_path += "/index.html";
    } else {
        file_path += path;
    }
    
    // Security check: prevent directory traversal
    std::filesystem::path canonical_static = std::filesystem::canonical(static_directory_);
    std::filesystem::path canonical_requested = std::filesystem::weakly_canonical(file_path);
    
    if (canonical_requested.string().find(canonical_static.string()) != 0) {
        response.setStatus(HttpResponse::BAD_REQUEST);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>400 Bad Request</h1><p>Invalid file path.</p></body></html>");
        return;
    }
    
    // Check if file exists
    if (!std::filesystem::exists(file_path) || !std::filesystem::is_regular_file(file_path)) {
        response.setStatus(HttpResponse::NOT_FOUND);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>404 Not Found</h1><p>File not found.</p></body></html>");
        return;
    }
    
    // Read file
    std::ifstream file(file_path, std::ios::binary);
    if (!file.is_open()) {
        response.setStatus(HttpResponse::INTERNAL_SERVER_ERROR);
        response.setContentType("text/html");
        response.setBody("<html><body><h1>500 Internal Server Error</h1><p>Unable to read file.</p></body></html>");
        return;
    }
    
    std::ostringstream content;
    content << file.rdbuf();
    
    response.setStatus(HttpResponse::OK);
    response.setContentType(getMimeType(file_path));
    response.setBody(content.str());
}

std::string HttpServer::getMimeType(const std::string& path) {
    std::filesystem::path file_path(path);
    std::string extension = file_path.extension().string();
    
    if (extension == ".html" || extension == ".htm") return "text/html";
    if (extension == ".css") return "text/css";
    if (extension == ".js") return "application/javascript";
    if (extension == ".json") return "application/json";
    if (extension == ".png") return "image/png";
    if (extension == ".jpg" || extension == ".jpeg") return "image/jpeg";
    if (extension == ".gif") return "image/gif";
    if (extension == ".svg") return "image/svg+xml";
    if (extension == ".txt") return "text/plain";
    
    return "application/octet-stream";
} 
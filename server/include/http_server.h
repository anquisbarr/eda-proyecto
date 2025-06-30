#pragma once

#include <string>
#include <unordered_map>
#include <functional>
#include <thread>
#include <vector>

class HttpRequest;
class HttpResponse;

class HttpServer {
public:
    using RouteHandler = std::function<void(const HttpRequest&, HttpResponse&)>;

    HttpServer(int port = 8080);
    ~HttpServer();

    void start();
    void stop();
    void addRoute(const std::string& method, const std::string& path, RouteHandler handler);
    void setStaticDirectory(const std::string& directory);

private:
    int port_;
    int server_socket_;
    bool running_;
    std::string static_directory_;
    std::unordered_map<std::string, RouteHandler> routes_;
    
    void handleClient(int client_socket);
    void processRequest(int client_socket);
    std::string getRouteKey(const std::string& method, const std::string& path);
    void serveStaticFile(const std::string& path, HttpResponse& response);
    std::string getMimeType(const std::string& path);
}; 
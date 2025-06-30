# C++ HTTP Web Server

A lightweight, multi-threaded HTTP web server built in C++ with CMake build system.

## Features

- **HTTP/1.1 Protocol Support**: Full HTTP/1.1 implementation
- **Static File Serving**: Serve HTML, CSS, JS, and other static files
- **RESTful API Routes**: Custom route handlers for API endpoints
- **Multi-threaded**: Handle multiple concurrent connections
- **MIME Type Detection**: Automatic content-type detection
- **Security**: Path traversal protection for static files
- **Cross-platform**: Works on Linux, macOS, and Windows

## Project Structure

```
.
├── CMakeLists.txt          # CMake build configuration
├── include/                # Header files
│   ├── http_server.h      # Main server class
│   ├── http_request.h     # HTTP request parser
│   └── http_response.h    # HTTP response builder
├── src/                   # Source files
│   ├── main.cpp           # Application entry point
│   ├── http_server.cpp    # Server implementation
│   ├── http_request.cpp   # Request implementation
│   └── http_response.cpp  # Response implementation
├── www/                   # Static web files
│   └── index.html         # Default homepage
└── README.md              # This file
```

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (GCC, Clang, or MSVC)
- POSIX-compatible system (Linux, macOS, Windows with WSL)

### Build Steps

1. Create a build directory:
```bash
mkdir build
cd build
```

2. Configure with CMake:
```bash
cmake ..
```

3. Build the project:
```bash
make
```

Or on Windows:
```bash
cmake --build .
```

## Running

From the build directory:
```bash
./http_server
```

The server will start on port 8080. Open your browser and navigate to:
- http://localhost:8080 - Homepage with API testing interface
- http://localhost:8080/api/hello - JSON API endpoint
- http://localhost:8080/api/info - Server information endpoint

## API Endpoints

### GET /api/hello
Returns a simple JSON greeting message.

**Response:**
```json
{
  "message": "Hello, World!",
  "method": "GET"
}
```

### GET /api/info
Returns server information.

**Response:**
```json
{
  "server": "C++ HTTP Server",
  "version": "1.0.0",
  "path": "/api/info"
}
```

### POST /api/echo
Echoes back the request body.

**Request:**
```json
{
  "message": "Hello from client!"
}
```

**Response:**
```json
{
  "echo": "{\"message\": \"Hello from client!\"}"
}
```

## Adding Custom Routes

You can add custom routes in `src/main.cpp`:

```cpp
httpServer.addRoute("GET", "/api/custom", [](const HttpRequest& req, HttpResponse& res) {
    res.setStatus(HttpResponse::OK);
    res.setContentType("application/json");
    res.setBody("{\"custom\": \"endpoint\"}");
});
```

## Configuration

- **Port**: Change the port in `src/main.cpp` (default: 8080)
- **Static Directory**: Modify the static directory path (default: "www")
- **Thread Pool**: Currently uses thread-per-connection model

## Stopping the Server

Press `Ctrl+C` to gracefully shutdown the server.

## Development

To extend the server:

1. Add new route handlers in `main.cpp`
2. Modify the `HttpServer` class for new features
3. Add static files to the `www/` directory
4. Rebuild with `make` in the build directory

## License

This project is open source and available under the MIT License. 
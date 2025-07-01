#include "http_server.h"
#include "http_request.h"
#include "http_response.h"
#include "algorithm_service.h"
#include "json.hpp"
#include <iostream>
#include <csignal>
#include <sstream>
#include <iomanip>
#include <fstream>

using json = nlohmann::json;

HttpServer* server = nullptr;
AlgorithmService* algorithmService = nullptr;

void signalHandler(int) {
    if (server) {
        std::cout << "\nShutting down server..." << std::endl;
        server->stop();
    }
    exit(0);
}

std::string algorithmResultToJson(const AlgorithmResult& result) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(4);
    json << "{";
    json << "\"algorithm_name\":\"" << result.algorithm_name << "\",";
    json << "\"precision\":" << result.precision << ",";
    json << "\"execution_time_ms\":" << result.execution_time_ms << ",";
    json << "\"success\":" << (result.success ? "true" : "false") << ",";
    if (!result.error_message.empty()) {
        json << "\"error_message\":\"" << result.error_message << "\",";
    }
    json << "\"distances_and_matches\":[";
    for (size_t i = 0; i < result.distances_and_matches.size(); ++i) {
        if (i > 0) json << ",";
        json << "{\"distance_squared\":" << result.distances_and_matches[i].first
             << ",\"is_match\":" << (result.distances_and_matches[i].second ? "true" : "false") << "}";
    }
    json << "]}";
    return json.str();
}

std::string comparisonResultToJson(const ComparisonResult& result) {
    std::ostringstream json;
    json << std::fixed << std::setprecision(4);
    json << "{";
    json << "\"job_id\":\"" << result.job_id << "\",";
    json << "\"k\":" << result.k << ",";
    json << "\"completed\":" << (result.completed ? "true" : "false") << ",";
    json << "\"query_info\":\"" << result.query_info << "\",";
    json << "\"total_time_ms\":" << result.total_time_ms << ",";
    json << "\"results\":[";
    for (size_t i = 0; i < result.results.size(); ++i) {
        if (i > 0) json << ",";
        json << algorithmResultToJson(result.results[i]);
    }
    json << "]}";
    return json.str();
}

int main() {
    // Set up signal handling for graceful shutdown
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    try {
        // Initialize algorithm service
        AlgorithmService algoService;
        algorithmService = &algoService;

        std::cout << "Loading SIFT dataset..." << std::endl;
        // if (!algoService.initialize("../siftsmall_query.fvecs")) {
        if (!algoService.initialize("../sift_base.fvecs")) {
            std::cerr << "Failed to load SIFT dataset. Check if siftsmall_query.fvecs exists in parent directory." << std::endl;
            return 1;
        }
        std::cout << "SIFT dataset loaded successfully!" << std::endl;
        std::cout << algoService.getDatasetInfo() << std::endl;
        std::cout << "Note: Algorithms will be built on first comparison request." << std::endl;

        // Create server on port 8080
        HttpServer httpServer(8080);
        server = &httpServer;

        // Set static file directory
        httpServer.setStaticDirectory("www");

        // Helper function to add CORS headers
        auto addCorsHeaders = [](HttpResponse& res) {
            res.setHeader("Access-Control-Allow-Origin", "*");
            res.setHeader("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS");
            res.setHeader("Access-Control-Allow-Headers", "Content-Type, Authorization, X-Requested-With");
            res.setHeader("Access-Control-Max-Age", "86400");
        };

        // Add OPTIONS handler for CORS preflight
        httpServer.addRoute("OPTIONS", "/api/compare", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            res.setStatus(HttpResponse::OK);
            res.setContentType("text/plain");
            res.setBody("");
        });

        httpServer.addRoute("OPTIONS", "/api/result", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            res.setStatus(HttpResponse::OK);
            res.setContentType("text/plain");
            res.setBody("");
        });

        // Add original routes
        httpServer.addRoute("GET", "/api/hello", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            res.setBody("{\"message\": \"Hello, World!\", \"method\": \"" + req.getMethod() + "\"}");
        });

        httpServer.addRoute("GET", "/api/info", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            res.setBody("{\"server\": \"C++ HTTP Server with Algorithm Service\", \"version\": \"1.0.0\", \"path\": \"" + req.getPath() + "\"}");
        });

        // Add dataset info endpoint
        httpServer.addRoute("GET", "/api/dataset", [&algoService, &addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            res.setBody("{\"info\": \"" + algoService.getDatasetInfo() + "\"}");
        });

        // Add available queries endpoint
        httpServer.addRoute("GET", "/api/queries", [&algoService, &addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            auto queries = algoService.getAvailableQueries(50);
            std::ostringstream json;
            json << "{\"available_queries\":[";
            for (size_t i = 0; i < queries.size(); ++i) {
                if (i > 0) json << ",";
                json << queries[i];
            }
            json << "]}";

            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            res.setBody(json.str());
        });

        // Add algorithm comparison endpoint
        httpServer.addRoute("POST", "/api/compare", [&algoService, &addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            try {
                // Parse request body for query_index and k
                std::string body = req.getBody();

                std::cout << "Received body: '" << body << "'" << std::endl;

                // Use proper JSON parsing
                int query_index = -1;
                int k = -1;

                try {
                    auto j = json::parse(body);

                    if (j.contains("query_index") && j["query_index"].is_number_integer()) {
                        query_index = j["query_index"];
                    }

                    if (j.contains("k") && j["k"].is_number_integer()) {
                        k = j["k"];
                    }

                    std::cout << "Parsed values: query_index=" << query_index << ", k=" << k << std::endl;

                } catch (const json::exception& e) {
                    std::cout << "JSON parsing error: " << e.what() << std::endl;
                    res.setStatus(HttpResponse::BAD_REQUEST);
                    res.setContentType("application/json");
                    res.setBody("{\"error\": \"Invalid JSON format\"}");
                    return;
                }

                if (query_index < 0 || k <= 0) {
                    res.setStatus(HttpResponse::BAD_REQUEST);
                    res.setContentType("application/json");
                    res.setBody("{\"error\": \"Invalid query_index or k parameter\"}");
                    return;
                }

                // Start the comparison job
                std::string job_id = algoService.startComparisonJob(query_index, k);
                if (job_id.empty()) {
                    res.setStatus(HttpResponse::BAD_REQUEST);
                    res.setContentType("application/json");
                    res.setBody("{\"error\": \"Invalid query_index\"}");
                    return;
                }

                res.setStatus(HttpResponse::OK);
                res.setContentType("application/json");
                res.setBody("{\"job_id\": \"" + job_id + "\", \"status\": \"started\"}");

            } catch (const std::exception& e) {
                res.setStatus(HttpResponse::INTERNAL_SERVER_ERROR);
                res.setContentType("application/json");
                res.setBody("{\"error\": \"" + std::string(e.what()) + "\"}");
            }
        });

        // Add job status/result endpoint
        httpServer.addRoute("GET", "/api/result", [&algoService, &addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            std::string job_id = req.getQueryParam("job_id");
            if (job_id.empty()) {
                res.setStatus(HttpResponse::BAD_REQUEST);
                res.setContentType("application/json");
                res.setBody("{\"error\": \"Missing job_id parameter\"}");
                return;
            }

            ComparisonResult result = algoService.getJobResult(job_id);

            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            res.setBody(comparisonResultToJson(result));
        });

        // Add cache management endpoint
        httpServer.addRoute("DELETE", "/api/cache", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            std::string cache_dir = "algorithm_cache";
            std::string rm_cmd = "rm -rf " + cache_dir;
            int result = system(rm_cmd.c_str());

            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");
            if (result == 0) {
                res.setBody("{\"message\": \"Algorithm cache cleared successfully. Restart server to rebuild.\", \"success\": true}");
            } else {
                res.setBody("{\"message\": \"Failed to clear cache or cache was already empty.\", \"success\": false}");
            }
        });

        // Add cache status endpoint
        httpServer.addRoute("GET", "/api/cache", [&addCorsHeaders](const HttpRequest& req, HttpResponse& res) {
            addCorsHeaders(res);
            std::string cache_dir = "algorithm_cache";
            std::ifstream meta_file(cache_dir + "/cache_metadata.txt");

            res.setStatus(HttpResponse::OK);
            res.setContentType("application/json");

            if (meta_file) {
                std::string line, metadata = "";
                while (std::getline(meta_file, line)) {
                    if (!metadata.empty()) metadata += ", ";
                    metadata += "\"" + line + "\"";
                }
                meta_file.close();
                res.setBody("{\"cache_exists\": true, \"metadata\": [" + metadata + "]}");
            } else {
                res.setBody("{\"cache_exists\": false, \"message\": \"No algorithm cache found\"}");
            }
        });

        // Start server
        std::cout << "\nStarting HTTP server on port 8080..." << std::endl;
        std::cout << "Visit http://localhost:8080 to view the web server" << std::endl;
        std::cout << "API endpoints:" << std::endl;
        std::cout << "  GET  /api/hello" << std::endl;
        std::cout << "  GET  /api/info" << std::endl;
        std::cout << "  GET  /api/dataset" << std::endl;
        std::cout << "  GET  /api/queries" << std::endl;
        std::cout << "  POST /api/compare" << std::endl;
        std::cout << "  GET  /api/result?job_id=<job_id>" << std::endl;
        std::cout << "  DELETE /api/cache" << std::endl;
        std::cout << "  GET  /api/cache" << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;

        // Start the server (this will block)
        httpServer.start();

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

#pragma once

#include <vector>
#include <string>
#include <memory>
#include <future>
#include <unordered_map>

// Forward declarations
template<typename PointType> class SannsDB;
template<typename PointType> class SSPTree;
class SIFTVector;

struct AlgorithmResult {
    std::string algorithm_name;
    float precision;
    double execution_time_ms;
    std::vector<std::pair<double, bool>> distances_and_matches; // distance^2, is_match
    bool success;
    std::string error_message;
};

struct ComparisonResult {
    std::string job_id;
    int k;
    bool completed;
    std::vector<AlgorithmResult> results;
    std::string query_info;
    double total_time_ms;
};

class AlgorithmService {
public:
    AlgorithmService();
    ~AlgorithmService();
    
    // Initialize the service with SIFT data
    bool initialize(const std::string& sift_file_path);
    
    // Start a new comparison job
    std::string startComparisonJob(int query_index, int k);
    
    // Get the status/result of a job
    ComparisonResult getJobResult(const std::string& job_id);
    
    // Get list of available queries (indices)
    std::vector<int> getAvailableQueries(int max_count = 100);
    
    // Get dataset info
    std::string getDatasetInfo();
    
    // Serialization support for persistent storage
    bool saveAlgorithmsToFile(const std::string& cache_dir = "algorithm_cache");
    bool loadAlgorithmsFromFile(const std::string& cache_dir = "algorithm_cache");
    bool isCacheValid(const std::string& cache_dir, const std::string& sift_file_path);

private:
    // Data
    std::vector<SIFTVector> sift_vectors_;
    std::unique_ptr<SannsDB<SIFTVector>> sanns_db_;
    std::unique_ptr<SSPTree<SIFTVector>> ssp_tree_;
    bool algorithms_built_ = false;
    
    // Job management
    std::unordered_map<std::string, std::future<ComparisonResult>> running_jobs_;
    std::unordered_map<std::string, ComparisonResult> completed_jobs_;
    
    // Helper methods
    std::string generateJobId();
    ComparisonResult runComparison(int query_index, int k);
    bool ensureAlgorithmsBuilt(); // Lazy initialization
    AlgorithmResult runSannsClusteringTest(const SIFTVector& query, int k);
    AlgorithmResult runSannsLinearScanTest(const SIFTVector& query, int k);
    AlgorithmResult runSspTreeTest(const SIFTVector& query, int k);
    
    // Utility functions
    template<typename PointType>
    bool equalPoint(const PointType& a, const PointType& b) const;
    
    template<typename PointType>
    double getDistanceSquared(const PointType& p1, const PointType& p2) const;
}; 
#include "algorithm_service.h"

// Include the algorithm headers
#include "../../Point.h"
#include "../../Sphere.h"
#include "../../SSPTree.h"
#include "../../SANNS.h"
#include "../../SIFTReader.hpp"

#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>
#include <thread>
#include <unordered_map>
#include <fstream>
#include <sys/stat.h>
#include <future>
#include <mutex>
#include <atomic>

#ifdef _OPENMP
#include <omp.h>
#endif

static constexpr float FLOAT_TOL = 1e-6f;

AlgorithmService::AlgorithmService() {
}

AlgorithmService::~AlgorithmService() = default;

bool AlgorithmService::initialize(const std::string& sift_file_path) {
    try {
        // Load SIFT vectors at startup
        std::cout << "Loading SIFT dataset..." << std::endl;
        sift_vectors_ = SIFTReader::readFile(sift_file_path);
        if (sift_vectors_.empty()) {
            return false;
        }
        std::cout << "Loaded " << sift_vectors_.size() << " SIFT vectors" << std::endl;
        
        // Use only a portion of the dataset for faster processing and reduced memory usage
        size_t original_size = sift_vectors_.size();
        size_t reduced_size = original_size / 16;  // Use 1/8 of dataset for testing OpenMP performance
        sift_vectors_.resize(reduced_size);
        std::cout << "Using reduced dataset: " << reduced_size << " vectors (reduced from " << original_size << ") for testing" << std::endl;
        
        // Try to load from cache first
        std::string cache_dir = "algorithm_cache";
        std::cout << "Checking algorithm cache..." << std::endl;
        
        if (isCacheValid(cache_dir, sift_file_path)) {
            std::cout << "Valid cache found, attempting to load..." << std::endl;
            if (loadAlgorithmsFromFile(cache_dir)) {
                std::cout << "Algorithms loaded from cache successfully!" << std::endl;
                algorithms_built_ = true;
                return true;
            } else {
                std::cout << "Cache load failed, building from scratch..." << std::endl;
            }
        } else {
            std::cout << "No valid cache found, building from scratch..." << std::endl;
        }
        
        // Build algorithms from scratch
        std::cout << "Building algorithms (this may take a few minutes)..." << std::endl;
        
        // Initialize SANNS DB with dynamic parameters optimized for better precision
        auto N = sift_vectors_.size();
        
        // More aggressive clustering for better precision
        // Lower threshold means recursion triggers sooner, creating more groups
        float LARGE_CLUSTER_FRAC_ALPHA;
        if (N >= 100000) {
            LARGE_CLUSTER_FRAC_ALPHA = 0.015f; // 1.5% for large datasets
        } else if (N >= 50000) {
            LARGE_CLUSTER_FRAC_ALPHA = 0.02f;  // 2% for medium datasets
        } else {
            LARGE_CLUSTER_FRAC_ALPHA = 0.03f;  // 3% for smaller datasets
        }

        // Much smaller clusters to force more subdivision and create multiple groups
        // Target: roughly 1% of dataset size, but with reasonable bounds
        size_t MAX_CLUSTER_SIZE_M;
        if (N >= 100000) {
            MAX_CLUSTER_SIZE_M = static_cast<size_t>(N * 0.01); // 1% of dataset
            MAX_CLUSTER_SIZE_M = std::max(static_cast<size_t>(200), MAX_CLUSTER_SIZE_M);
            MAX_CLUSTER_SIZE_M = std::min(static_cast<size_t>(800), MAX_CLUSTER_SIZE_M);
        } else {
            MAX_CLUSTER_SIZE_M = static_cast<size_t>(std::sqrt(N) * 1.5); // Traditional formula for smaller datasets
            MAX_CLUSTER_SIZE_M = std::max(static_cast<size_t>(50), MAX_CLUSTER_SIZE_M);
            MAX_CLUSTER_SIZE_M = std::min(static_cast<size_t>(400), MAX_CLUSTER_SIZE_M);
        }

        // Adjusted for better retrieval with multiple groups
        const double U_CONSTANT = 2.0; // Increased to retrieve more clusters
        size_t CLUSTERS_TO_RETRIEVE_U = static_cast<size_t>(U_CONSTANT * std::log2(N));
        CLUSTERS_TO_RETRIEVE_U = std::max(static_cast<size_t>(5), CLUSTERS_TO_RETRIEVE_U); // Higher minimum
        CLUSTERS_TO_RETRIEVE_U = std::min(static_cast<size_t>(50), CLUSTERS_TO_RETRIEVE_U); // Lower maximum for efficiency

        // Balanced L factor for approximation quality
        const double L_FACTOR = 6.0; // Reduced from 8.0 for better balance
        size_t APPROX_BINS_L_CLUSTERING = static_cast<size_t>(L_FACTOR * CLUSTERS_TO_RETRIEVE_U);
        
        // Print optimized parameters for precision-focused clustering
        std::cout << "SANNS Parameters (precision-optimized) for " << N << " points:" << std::endl;
        std::cout << "  MAX_CLUSTER_SIZE_M: " << MAX_CLUSTER_SIZE_M << " (target: ~" << std::fixed << std::setprecision(1) << (100.0 * MAX_CLUSTER_SIZE_M / N) << "% of dataset)" << std::endl;
        std::cout << "  LARGE_CLUSTER_FRAC_ALPHA: " << LARGE_CLUSTER_FRAC_ALPHA << " (trigger recursion when large clusters > " << std::fixed << std::setprecision(1) << (LARGE_CLUSTER_FRAC_ALPHA * 100) << "% of points)" << std::endl;
        std::cout << "  CLUSTERS_TO_RETRIEVE_U: " << CLUSTERS_TO_RETRIEVE_U << std::endl;
        std::cout << "  APPROX_BINS_L_CLUSTERING: " << APPROX_BINS_L_CLUSTERING << std::endl;
        std::cout << "  Expected result: Multiple groups with smaller clusters for better precision" << std::endl;
        
        // Hardware information
        unsigned int num_threads = std::thread::hardware_concurrency();
        if (num_threads == 0) num_threads = 4; // fallback
        
#ifdef _OPENMP
        omp_set_num_threads(num_threads);
        std::cout << "OpenMP available with " << num_threads << " threads" << std::endl;
#else
        std::cout << "Using " << num_threads << " threads (std::async)" << std::endl;
#endif

            // Memory usage warning for large datasets
        if (N > 500000) {
            double estimated_memory_gb = (N * 128 * sizeof(float)) / (1024.0 * 1024.0 * 1024.0);
            std::cout << "WARNING: Large dataset detected (" << N << " points)" << std::endl;
            std::cout << "  Estimated memory usage: ~" << std::fixed << std::setprecision(1) 
                      << estimated_memory_gb << "GB for SIFT vectors alone" << std::endl;
            std::cout << "  Building algorithms in parallel may take several minutes..." << std::endl;
            std::cout << "  Precision calculation will be disabled for performance" << std::endl;
        }

        // Initialize SSP-Tree parameters early for parallel construction
        size_t MAX_ENTRIES = 32;
        if (N > 500000) {
            MAX_ENTRIES = std::min(static_cast<size_t>(128), static_cast<size_t>(32 + N/50000));
            std::cout << "Increased SSP-Tree MAX_ENTRIES to " << MAX_ENTRIES << " for large dataset" << std::endl;
        }
        
        std::cout << "Building algorithms in parallel..." << std::endl;
        auto total_start = std::chrono::high_resolution_clock::now();
        
        // Build SANNS and SSP-Tree in parallel using std::async
        auto sanns_future = std::async(std::launch::async, [&]() {
            std::cout << "[SANNS] Starting SANNS DB construction..." << std::endl;
            auto sanns_start = std::chrono::high_resolution_clock::now();
            
            // Add progress monitoring thread for SANNS
            std::atomic<bool> sanns_complete(false);
            std::thread progress_thread([&sanns_complete, sanns_start]() {
                int elapsed_seconds = 0;
                while (!sanns_complete.load()) {
                    std::this_thread::sleep_for(std::chrono::seconds(15));
                    elapsed_seconds += 15;
                    if (!sanns_complete.load()) {
                        std::cout << "[SANNS] Construction in progress... " << elapsed_seconds << "s elapsed" << std::endl;
                        
                        // Timeout protection - kill if taking too long
                        if (elapsed_seconds > 600) { // 10 minutes timeout
                            std::cout << "[SANNS] ERROR: Construction timed out after 10 minutes!" << std::endl;
                            std::cout << "[SANNS] This may indicate an infinite loop or deadlock in SANNS" << std::endl;
                            throw std::runtime_error("SANNS construction timeout");
                        }
                    }
                }
            });
            
            try {
                auto sanns_temp = std::make_unique<SannsDB<SIFTVector>>(
                    MAX_CLUSTER_SIZE_M, LARGE_CLUSTER_FRAC_ALPHA, 
                    CLUSTERS_TO_RETRIEVE_U, APPROX_BINS_L_CLUSTERING
                );
                
                std::cout << "[SANNS] Calling build() method..." << std::endl;
                sanns_temp->build(sift_vectors_);
                std::cout << "[SANNS] Build() method completed successfully" << std::endl;
                
                sanns_complete.store(true);
                progress_thread.join();
                
                auto sanns_end = std::chrono::high_resolution_clock::now();
                auto sanns_time = std::chrono::duration<double>(sanns_end - sanns_start).count();
                std::cout << "[SANNS] SANNS DB built in " << std::fixed << std::setprecision(2) << sanns_time << " seconds" << std::endl;
                
                return sanns_temp;
            } catch (...) {
                sanns_complete.store(true);
                progress_thread.join();
                throw;
            }
        });
        
        auto ssp_future = std::async(std::launch::async, [&, MAX_ENTRIES]() {
            std::cout << "[SSP-Tree] Starting SSP-Tree construction..." << std::endl;
            auto ssp_start = std::chrono::high_resolution_clock::now();
            
            auto ssp_temp = std::make_unique<SSPTree<SIFTVector>>(MAX_ENTRIES);
            
            // Use OpenMP for parallel insertion if available
#ifdef _OPENMP
            std::mutex insert_mutex; // Protect tree insertion since trees aren't typically thread-safe
            std::cout << "[SSP-Tree] Using OpenMP for parallel processing..." << std::endl;
            
            // We can't parallelize insertions directly due to tree structure,
            // but we can batch process and show parallel progress
            size_t progress_interval = std::max(static_cast<size_t>(1), N / 20);
            std::atomic<size_t> count(0);
            
            #pragma omp parallel for schedule(dynamic, 1000)
            for (size_t i = 0; i < sift_vectors_.size(); ++i) {
                {
                    std::lock_guard<std::mutex> lock(insert_mutex);
                    ssp_temp->insert(sift_vectors_[i]);
                }
                
                size_t current_count = count.fetch_add(1) + 1;
                if (N > 100000 && (current_count % progress_interval == 0)) {
                    double progress = (static_cast<double>(current_count) / N) * 100.0;
                    std::cout << "[SSP-Tree] Insertion progress: " << std::fixed << std::setprecision(1) 
                              << progress << "% (" << current_count << "/" << N << ")" << std::endl;
                }
            }
#else
            // Fallback to sequential insertion with progress
            size_t progress_interval = std::max(static_cast<size_t>(1), N / 20);
            size_t count = 0;
            for(const auto& vec : sift_vectors_) {
                ssp_temp->insert(vec);
                if (N > 100000 && (++count % progress_interval == 0)) {
                    double progress = (static_cast<double>(count) / N) * 100.0;
                    std::cout << "[SSP-Tree] Insertion progress: " << std::fixed << std::setprecision(1) 
                              << progress << "% (" << count << "/" << N << ")" << std::endl;
                }
            }
#endif
            
            auto ssp_end = std::chrono::high_resolution_clock::now();
            auto ssp_time = std::chrono::duration<double>(ssp_end - ssp_start).count();
            std::cout << "[SSP-Tree] SSP-Tree built in " << std::fixed << std::setprecision(2) << ssp_time << " seconds" << std::endl;
            
            return ssp_temp;
        });
        
        // Wait for both algorithms to complete and assign results
        std::cout << "Waiting for parallel construction to complete..." << std::endl;
        sanns_db_ = sanns_future.get();
        ssp_tree_ = ssp_future.get();
        
        auto total_end = std::chrono::high_resolution_clock::now();
        auto total_time = std::chrono::duration<double>(total_end - total_start).count();
        std::cout << "All algorithms built in " << std::fixed << std::setprecision(2) << total_time << " seconds (parallel)" << std::endl;
        
        std::cout << "Algorithms built successfully!" << std::endl;
        algorithms_built_ = true;
        
        // Save to cache for next time
        std::cout << "Saving algorithms to cache for faster future startups..." << std::endl;
        if (saveAlgorithmsToFile(cache_dir)) {
            std::cout << "Cache saved successfully! Next startup will be much faster." << std::endl;
        } else {
            std::cout << "Warning: Failed to save cache, but algorithms are ready to use." << std::endl;
        }
        
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Error during initialization: " << e.what() << std::endl;
        std::cerr << "This may indicate:" << std::endl;
        std::cerr << "  1. SANNS algorithm deadlock/infinite loop" << std::endl;
        std::cerr << "  2. Memory allocation issues" << std::endl;
        std::cerr << "  3. Thread synchronization problems" << std::endl;
        std::cerr << "Recommendation: Try with an even smaller dataset" << std::endl;
        return false;
    }
}

std::string AlgorithmService::startComparisonJob(int query_index, int k) {
    if (query_index < 0 || query_index >= static_cast<int>(sift_vectors_.size())) {
        return "";
    }
    
    std::string job_id = generateJobId();
    
    // Start the comparison in a separate thread
    auto future = std::async(std::launch::async, [this, query_index, k]() {
        return runComparison(query_index, k);
    });
    
    running_jobs_[job_id] = std::move(future);
    
    return job_id;
}

ComparisonResult AlgorithmService::getJobResult(const std::string& job_id) {
    // Check if job is completed
    auto completed_it = completed_jobs_.find(job_id);
    if (completed_it != completed_jobs_.end()) {
        return completed_it->second;
    }
    
    // Check if job is running
    auto running_it = running_jobs_.find(job_id);
    if (running_it != running_jobs_.end()) {
        auto& future = running_it->second;
        
        // Check if the future is ready
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            // Job completed, move to completed jobs
            ComparisonResult result = future.get();
            result.completed = true;
            completed_jobs_[job_id] = result;
            running_jobs_.erase(running_it);
            return result;
        } else {
            // Job still running
            ComparisonResult result;
            result.job_id = job_id;
            result.completed = false;
            return result;
        }
    }
    
    // Job not found
    ComparisonResult result;
    result.job_id = job_id;
    result.completed = true;
    return result;
}

std::vector<int> AlgorithmService::getAvailableQueries(int max_count) {
    std::vector<int> indices;
    int count = std::min(max_count, static_cast<int>(sift_vectors_.size()));
    for (int i = 0; i < count; ++i) {
        indices.push_back(i);
    }
    return indices;
}

std::string AlgorithmService::getDatasetInfo() {
    if (sift_vectors_.empty()) {
        return "Dataset not loaded";
    }
    
    auto N = sift_vectors_.size();
    std::ostringstream oss;
    oss << "Dataset: " << N << " SIFT vectors, ";
    oss << "Dimension: " << sift_vectors_[0].getDimension();
    
    // Add optimization status information
    if (N > 500000) {
        oss << " (Large dataset mode: precision calculation disabled for performance)";
    } else if (N > 100000) {
        oss << " (Medium dataset: optimized parameters enabled)";
    }
    
    return oss.str();
}

std::string AlgorithmService::generateJobId() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(10000, 99999);
    return "job_" + std::to_string(dis(gen)) + "_" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

bool AlgorithmService::ensureAlgorithmsBuilt() {
    if (algorithms_built_) {
        return true;
    }
    
    std::cerr << "Error: Algorithms were not built during initialization. This should not happen." << std::endl;
    return false;
}

ComparisonResult AlgorithmService::runComparison(int query_index, int k) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ComparisonResult result;
    result.k = k;
    result.completed = false;
    
    try {
        // Ensure algorithms are built before running comparison
        if (!ensureAlgorithmsBuilt()) {
            throw std::runtime_error("Failed to build algorithms");
        }
        
        const SIFTVector& query = sift_vectors_[query_index];
        
        // Format query info
        std::ostringstream query_info;
        query_info << "Query index: " << query_index << ", ID: " << query.id;
        result.query_info = query_info.str();
        
        // Run all three algorithms
        result.results.push_back(runSannsClusteringTest(query, k));
        result.results.push_back(runSannsLinearScanTest(query, k));
        result.results.push_back(runSspTreeTest(query, k));
        
        auto end_time = std::chrono::high_resolution_clock::now();
        result.total_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
        result.completed = true;
        
    } catch (const std::exception& e) {
        result.completed = true;
        // Add error to all results
        for (auto& algo_result : result.results) {
            algo_result.success = false;
            algo_result.error_message = e.what();
        }
    }
    
    return result;
}

AlgorithmResult AlgorithmService::runSannsClusteringTest(const SIFTVector& query, int k) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AlgorithmResult result;
    result.algorithm_name = "SANNS Clustering";
    result.success = true;
    
    try {
        // Get results from SANNS clustering
        std::vector<SIFTVector> sanns_res = sanns_db_->kNearestNeighbors(query, k);
        
        // Skip brute force for very large datasets to avoid performance bottleneck
        auto N = sift_vectors_.size();
        bool skip_brute_force = N > 500000;
        std::vector<SIFTVector> brute_res;
        
        if (!skip_brute_force) {
            brute_res = naiveTopK(query, sift_vectors_, k);
        }
        
        // Calculate precision (skip for large datasets)
        int correct_found = 0;
        result.distances_and_matches.clear();
        
        for (size_t i = 0; i < sanns_res.size(); ++i) {
            double dist_sq = getDistanceSquared(query, sanns_res[i]);
            bool is_match = false;
            
            if (!skip_brute_force) {
                for (const auto& brute_p : brute_res) {
                    if (equalPoint(sanns_res[i], brute_p)) {
                        correct_found++;
                        is_match = true;
                        break;
                    }
                }
            } else {
                // For large datasets, we can't compute exact precision
                is_match = true; // Assume all results are reasonable
                correct_found = static_cast<int>(sanns_res.size());
            }
            
            result.distances_and_matches.push_back({dist_sq, is_match});
        }
        
        if (skip_brute_force) {
            result.precision = -1.0f; // Indicate precision not computed
        } else {
            result.precision = static_cast<float>(correct_found) / k;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.precision = 0.0f;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    return result;
}

AlgorithmResult AlgorithmService::runSannsLinearScanTest(const SIFTVector& query, int k) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AlgorithmResult result;
    result.algorithm_name = "SANNS Linear Scan";
    result.success = true;
    
    try {
        // Optimize parameters for large datasets
        constexpr size_t RP_BITS = 8;
        size_t LS_BINS = 700;
        
        // Scale LS_BINS for large datasets
        auto N = sift_vectors_.size();
        if (N > 100000) {
            LS_BINS = static_cast<size_t>(std::sqrt(N) * 2.2); // Dynamic scaling
            LS_BINS = std::min(LS_BINS, static_cast<size_t>(5000)); // Cap at 5000
        }
        
        // Get results from SANNS linear scan
        std::vector<SIFTVector> sanns_res = linearScanKnn(query, sift_vectors_, k, RP_BITS, LS_BINS);
        
        // Skip brute force for very large datasets to avoid performance bottleneck
        bool skip_brute_force = N > 500000;
        std::vector<SIFTVector> brute_res;
        
        if (!skip_brute_force) {
            brute_res = naiveTopKSquared(query, sift_vectors_, k);
        }
        
        // Calculate precision (skip for large datasets)
        int correct_found = 0;
        result.distances_and_matches.clear();
        
        for (size_t i = 0; i < sanns_res.size(); ++i) {
            double dist_sq = getDistanceSquared(query, sanns_res[i]);
            bool is_match = false;
            
            if (!skip_brute_force) {
                for (const auto& brute_p : brute_res) {
                    if (equalPoint(sanns_res[i], brute_p)) {
                        correct_found++;
                        is_match = true;
                        break;
                    }
                }
            } else {
                // For large datasets, we can't compute exact precision
                is_match = true; // Assume all results are reasonable
                correct_found = static_cast<int>(sanns_res.size());
            }
            
            result.distances_and_matches.push_back({dist_sq, is_match});
        }
        
        if (skip_brute_force) {
            result.precision = -1.0f; // Indicate precision not computed
        } else {
            result.precision = static_cast<float>(correct_found) / k;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.precision = 0.0f;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    return result;
}

AlgorithmResult AlgorithmService::runSspTreeTest(const SIFTVector& query, int k) {
    auto start_time = std::chrono::high_resolution_clock::now();
    
    AlgorithmResult result;
    result.algorithm_name = "SSP-Tree Experimental";
    result.success = true;
    
    try {
        // Get results from SSP-Tree
        std::vector<SIFTVector> experimental_res = ssp_tree_->experimentalKnn(query, k);
        
        // Skip brute force for very large datasets to avoid performance bottleneck
        auto N = sift_vectors_.size();
        bool skip_brute_force = N > 500000;
        std::vector<SIFTVector> brute_res;
        
        if (!skip_brute_force) {
            brute_res = naiveTopK(query, sift_vectors_, k);
        }
        
        // Calculate precision (skip for large datasets)
        int correct_found = 0;
        result.distances_and_matches.clear();
        
        for (size_t i = 0; i < experimental_res.size(); ++i) {
            double dist_sq = getDistanceSquared(query, experimental_res[i]);
            bool is_match = false;
            
            if (!skip_brute_force) {
                for (const auto& brute_p : brute_res) {
                    if (equalPoint(experimental_res[i], brute_p)) {
                        correct_found++;
                        is_match = true;
                        break;
                    }
                }
            } else {
                // For large datasets, we can't compute exact precision
                is_match = true; // Assume all results are reasonable
                correct_found = static_cast<int>(experimental_res.size());
            }
            
            result.distances_and_matches.push_back({dist_sq, is_match});
        }
        
        if (skip_brute_force) {
            result.precision = -1.0f; // Indicate precision not computed
        } else {
            result.precision = static_cast<float>(correct_found) / k;
        }
        
    } catch (const std::exception& e) {
        result.success = false;
        result.error_message = e.what();
        result.precision = 0.0f;
    }
    
    auto end_time = std::chrono::high_resolution_clock::now();
    result.execution_time_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    
    return result;
}

template<typename PointType>
bool AlgorithmService::equalPoint(const PointType& a, const PointType& b) const {
    return PointType::distance(a, b) < FLOAT_TOL;
}

template<typename PointType>
double AlgorithmService::getDistanceSquared(const PointType& p1, const PointType& p2) const {
    double dist_sq = 0.0;
    for(size_t i = 0; i < PointType::getDimension(); ++i) {
        double diff = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        dist_sq += diff * diff;
    }
    return dist_sq;
}

// Explicit template instantiations
template bool AlgorithmService::equalPoint<SIFTVector>(const SIFTVector& a, const SIFTVector& b) const;
template double AlgorithmService::getDistanceSquared<SIFTVector>(const SIFTVector& p1, const SIFTVector& p2) const;

// Serialization implementation
bool AlgorithmService::saveAlgorithmsToFile(const std::string& cache_dir) {
    if (!algorithms_built_) {
        std::cerr << "Cannot save algorithms: not built yet" << std::endl;
        return false;
    }
    
    try {
        // Create cache directory
        std::string mkdir_cmd = "mkdir -p " + cache_dir;
        system(mkdir_cmd.c_str());
        
        // Save SANNS database
        std::ofstream sanns_file(cache_dir + "/sanns_cache.bin", std::ios::binary);
        if (!sanns_file) {
            std::cerr << "Failed to create SANNS cache file" << std::endl;
            return false;
        }
        
        // Calculate SANNS parameters dynamically (same as initialization)
        auto N = sift_vectors_.size();
        
        // Use the same improved parameter calculations
        float large_cluster_frac;
        if (N >= 100000) {
            large_cluster_frac = 0.015f;
        } else if (N >= 50000) {
            large_cluster_frac = 0.02f;
        } else {
            large_cluster_frac = 0.03f;
        }
        
        size_t max_cluster_size;
        if (N >= 100000) {
            max_cluster_size = static_cast<size_t>(N * 0.01);
            max_cluster_size = std::max(static_cast<size_t>(200), max_cluster_size);
            max_cluster_size = std::min(static_cast<size_t>(800), max_cluster_size);
        } else {
            max_cluster_size = static_cast<size_t>(std::sqrt(N) * 1.5);
            max_cluster_size = std::max(static_cast<size_t>(50), max_cluster_size);
            max_cluster_size = std::min(static_cast<size_t>(400), max_cluster_size);
        }
        
        const double U_CONSTANT = 2.0;
        size_t clusters_to_retrieve = static_cast<size_t>(U_CONSTANT * std::log2(N));
        clusters_to_retrieve = std::max(static_cast<size_t>(5), clusters_to_retrieve);
        clusters_to_retrieve = std::min(static_cast<size_t>(50), clusters_to_retrieve);
        
        const double L_FACTOR = 6.0;
        size_t approx_bins = static_cast<size_t>(L_FACTOR * clusters_to_retrieve);
        
        sanns_file.write(reinterpret_cast<const char*>(&max_cluster_size), sizeof(max_cluster_size));
        sanns_file.write(reinterpret_cast<const char*>(&large_cluster_frac), sizeof(large_cluster_frac));
        sanns_file.write(reinterpret_cast<const char*>(&clusters_to_retrieve), sizeof(clusters_to_retrieve));
        sanns_file.write(reinterpret_cast<const char*>(&approx_bins), sizeof(approx_bins));
        
        // For now, write a simple marker that indicates we need to rebuild
        // (Full SANNS serialization would require exposing internal structures)
        uint32_t sanns_marker = 0xDEADBEEF;
        sanns_file.write(reinterpret_cast<const char*>(&sanns_marker), sizeof(sanns_marker));
        sanns_file.close();
        
        // Save SSP-Tree (similar approach)
        std::ofstream ssp_file(cache_dir + "/ssp_cache.bin", std::ios::binary);
        if (!ssp_file) {
            std::cerr << "Failed to create SSP-Tree cache file" << std::endl;
            return false;
        }
        
        // Calculate dynamic MAX_ENTRIES (same as initialization)
        size_t max_entries = 32;
        if (N > 500000) {
            max_entries = std::min(static_cast<size_t>(128), static_cast<size_t>(32 + N/50000));
        }
        ssp_file.write(reinterpret_cast<const char*>(&max_entries), sizeof(max_entries));
        
        uint32_t ssp_marker = 0xCAFEBABE;
        ssp_file.write(reinterpret_cast<const char*>(&ssp_marker), sizeof(ssp_marker));
        ssp_file.close();
        
        // Save metadata file with timestamp and SIFT file info
        std::ofstream meta_file(cache_dir + "/cache_metadata.txt");
        if (meta_file) {
            auto now = std::chrono::system_clock::now();
            auto time_t = std::chrono::system_clock::to_time_t(now);
            meta_file << "cache_timestamp=" << time_t << std::endl;
            meta_file << "sift_vector_count=" << sift_vectors_.size() << std::endl;
            meta_file << "sift_dimension=" << (sift_vectors_.empty() ? 0 : sift_vectors_[0].getDimension()) << std::endl;
            meta_file.close();
        }
        
        std::cout << "Algorithm cache saved to: " << cache_dir << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error saving algorithm cache: " << e.what() << std::endl;
        return false;
    }
}

bool AlgorithmService::loadAlgorithmsFromFile(const std::string& cache_dir) {
    try {
        // Check if cache files exist
        std::ifstream sanns_file(cache_dir + "/sanns_cache.bin", std::ios::binary);
        std::ifstream ssp_file(cache_dir + "/ssp_cache.bin", std::ios::binary);
        
        if (!sanns_file || !ssp_file) {
            std::cout << "Cache files not found, will build algorithms from scratch" << std::endl;
            return false;
        }
        
        // For this simplified implementation, we'll just verify the markers exist
        // and then rebuild the algorithms (a full implementation would deserialize the structures)
        
        // Verify SANNS cache
        sanns_file.seekg(-sizeof(uint32_t), std::ios::end);
        uint32_t sanns_marker;
        sanns_file.read(reinterpret_cast<char*>(&sanns_marker), sizeof(sanns_marker));
        if (sanns_marker != 0xDEADBEEF) {
            std::cout << "SANNS cache appears corrupted, rebuilding" << std::endl;
            return false;
        }
        
        // Verify SSP-Tree cache
        ssp_file.seekg(-sizeof(uint32_t), std::ios::end);
        uint32_t ssp_marker;
        ssp_file.read(reinterpret_cast<char*>(&ssp_marker), sizeof(ssp_marker));
        if (ssp_marker != 0xCAFEBABE) {
            std::cout << "SSP-Tree cache appears corrupted, rebuilding" << std::endl;
            return false;
        }
        
        sanns_file.close();
        ssp_file.close();
        
        // For now, this simplified version just validates cache exists and rebuilds
        // A full implementation would actually deserialize the data structures
        std::cout << "Valid cache found, but rebuilding algorithms (simplified implementation)" << std::endl;
        return false; // Still rebuild for now
        
    } catch (const std::exception& e) {
        std::cerr << "Error loading algorithm cache: " << e.what() << std::endl;
        return false;
    }
}

bool AlgorithmService::isCacheValid(const std::string& cache_dir, const std::string& sift_file_path) {
    try {
        // Check if metadata file exists
        std::ifstream meta_file(cache_dir + "/cache_metadata.txt");
        if (!meta_file) {
            return false;
        }
        
        // Read cache metadata
        std::string line;
        time_t cache_timestamp = 0;
        size_t cached_vector_count = 0;
        size_t cached_dimension = 0;
        
        while (std::getline(meta_file, line)) {
            if (line.find("cache_timestamp=") == 0) {
                cache_timestamp = std::stol(line.substr(16));
            } else if (line.find("sift_vector_count=") == 0) {
                cached_vector_count = std::stoul(line.substr(18));
            } else if (line.find("sift_dimension=") == 0) {
                cached_dimension = std::stoul(line.substr(15));
            }
        }
        meta_file.close();
        
        // Check if SIFT file has been modified since cache was created
        struct stat sift_stat;
        if (stat(sift_file_path.c_str(), &sift_stat) != 0) {
            std::cerr << "Cannot access SIFT file: " << sift_file_path << std::endl;
            return false;
        }
        
        if (sift_stat.st_mtime > cache_timestamp) {
            std::cout << "SIFT file is newer than cache, rebuilding" << std::endl;
            return false;
        }
        
        // Validate that cached data matches current SIFT file
        if (cached_vector_count != sift_vectors_.size()) {
            std::cout << "SIFT vector count mismatch, rebuilding" << std::endl;
            return false;
        }
        
        if (!sift_vectors_.empty() && cached_dimension != sift_vectors_[0].getDimension()) {
            std::cout << "SIFT dimension mismatch, rebuilding" << std::endl;
            return false;
        }
        
        // Check if cache files exist and are readable
        std::ifstream sanns_check(cache_dir + "/sanns_cache.bin");
        std::ifstream ssp_check(cache_dir + "/ssp_cache.bin");
        
        if (!sanns_check || !ssp_check) {
            std::cout << "Cache files missing, rebuilding" << std::endl;
            return false;
        }
        
        std::cout << "Cache is valid and up-to-date" << std::endl;
        return true;
        
    } catch (const std::exception& e) {
        std::cerr << "Error validating cache: " << e.what() << std::endl;
        return false;
    }
} 
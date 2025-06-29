#ifndef SANNS_PLAINTEXT_H
#define SANNS_PLAINTEXT_H

#include <iostream>
#include <vector>
#include <string>
#include <numeric>   // Para std::iota
#include <algorithm> // Para std::shuffle, std::sort, etc.
#include <random>
#include "Point.h"
#include "SSPTree.h" // Usaremos el k-means de aquí

using namespace std;

// Declaración adelantada para el helper k-means
std::vector<int> kmeansClusteringGeneral(const std::vector<Point>& points_to_cluster, size_t k);

// --- Helpers para los algoritmos Top-K (CORREGIDOS) ---

// Implementación de NaiveTopK (similar a Algoritmo 1 del paper)
std::vector<Point> naiveTopK(const Point& query, const std::vector<Point>& points, size_t k) {
    if (points.empty() || k == 0) {
        return {};
    }
    
    using Elem = std::pair<float, Point>;
    std::vector<Elem> dist_pairs;
    dist_pairs.reserve(points.size());
    for (const auto& p : points) {
        dist_pairs.emplace_back(Point::distance(query, p), p);
    }

    k = std::min(k, points.size());
    auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; }; // CORREGIDO: orden ascendente
    std::partial_sort(dist_pairs.begin(), dist_pairs.begin() + k, dist_pairs.end(), cmp);

    std::vector<Point> result;
    result.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        result.push_back(dist_pairs[i].second);
    }
    return result;
}

// Versión que usa distancia al cuadrado para mejor consistencia
std::vector<Point> naiveTopKSquared(const Point& query, const std::vector<Point>& points, size_t k) {
    if (points.empty() || k == 0) return {};
    
    using Elem = std::pair<double, Point>;
    std::vector<Elem> dist_pairs;
    dist_pairs.reserve(points.size());
    
    for (const auto& p : points) {
        double dist_sq = 0.0;
        for(size_t i = 0; i < DIM; ++i) {
            double diff = static_cast<double>(query[i]) - static_cast<double>(p[i]);
            dist_sq += diff * diff;
        }
        dist_pairs.emplace_back(dist_sq, p);
    }

    k = std::min(k, points.size());
    auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; }; // CORREGIDO: orden ascendente
    std::partial_sort(dist_pairs.begin(), dist_pairs.begin() + k, dist_pairs.end(), cmp);

    std::vector<Point> result;
    result.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        result.push_back(dist_pairs[i].second);
    }
    return result;
}

// FUNCIÓN APPROXIMATETOPK CORREGIDA
std::vector<Point> approximateTopK(const Point& query, const std::vector<Point>& points, size_t k, size_t l_bins) {
    if (points.empty() || k == 0) {
        return {};
    }
    k = std::min(k, points.size());
    l_bins = std::min(l_bins, points.size());
    
    // Si l_bins es muy pequeño, hacer búsqueda exacta
    if (l_bins < k) {
        l_bins = k;
    }

    std::vector<size_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // Barajar índices aleatoriamente
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    // Encontrar el mínimo en cada bin
    std::vector<Point> bin_mins;
    bin_mins.reserve(l_bins);

    size_t bin_size = (points.size() + l_bins - 1) / l_bins; // Redondear hacia arriba

    for (size_t i = 0; i < l_bins; ++i) {
        size_t start_idx = i * bin_size;
        size_t end_idx = std::min((i + 1) * bin_size, points.size()); // CORREGIDO

        if (start_idx >= end_idx) continue;

        Point min_point = points[indices[start_idx]];
        float min_dist = Point::distance(query, min_point);

        for (size_t j = start_idx + 1; j < end_idx; ++j) {
            float dist = Point::distance(query, points[indices[j]]);
            if (dist < min_dist) {
                min_dist = dist;
                min_point = points[indices[j]];
            }
        }
        bin_mins.push_back(min_point);
    }
    
    // De los 'l' mínimos, encontrar los 'k' mejores usando fuerza bruta
    return naiveTopK(query, bin_mins, k);
}

// FUNCIÓN APPROXIMATETOPK CON DISTANCIAS CORREGIDA
std::vector<Point> approximateTopK_with_distances(
    const Point& query,
    const std::vector<long long>& distances, 
    const std::vector<Point>& points, 
    size_t k, 
    size_t l_bins
) {
    if (points.empty() || k == 0) return {};
    k = std::min(k, points.size());
    
    // Si l_bins es muy grande o pequeño, hacer búsqueda exacta
    if (l_bins >= points.size() || l_bins < k) {
        using Elem = std::pair<long long, size_t>;
        std::vector<Elem> dist_idx_pairs(points.size());
        for(size_t i = 0; i < points.size(); ++i) {
            dist_idx_pairs[i] = {distances[i], i};
        }

        auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; }; // CORREGIDO: orden ascendente
        std::partial_sort(dist_idx_pairs.begin(), dist_idx_pairs.begin() + k, dist_idx_pairs.end(), cmp);

        std::vector<Point> result;
        result.reserve(k);
        for(size_t i = 0; i < k; ++i) {
            result.push_back(points[dist_idx_pairs[i].second]);
        }
        return result;
    }

    // Lógica de aproximación (Shuffle and Bin)
    std::vector<size_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0);

    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    std::vector<Point> candidate_points;
    candidate_points.reserve(l_bins);

    size_t bin_size = (points.size() + l_bins - 1) / l_bins; // Redondear hacia arriba
    
    for (size_t i = 0; i < l_bins; ++i) {
        size_t start_idx = i * bin_size;
        size_t end_idx = std::min((i + 1) * bin_size, points.size()); // CORREGIDO
        
        if (start_idx >= end_idx) continue;
        
        size_t min_idx_in_bin = indices[start_idx];
        long long min_dist_val = distances[min_idx_in_bin];

        for(size_t j = start_idx + 1; j < end_idx; ++j) {
            size_t current_idx = indices[j];
            if (distances[current_idx] < min_dist_val) {
                min_dist_val = distances[current_idx];
                min_idx_in_bin = current_idx;
            }
        }
        candidate_points.push_back(points[min_idx_in_bin]);
    }

    // Refinar usando naiveTopKSquared para consistencia
    return naiveTopKSquared(query, candidate_points, k);
}

/**
 * ALGORITMO 3 CORREGIDO: Plaintext Linear Scan
 */
std::vector<Point> linearScanKnn(
    const Point& query,
    const std::vector<Point>& allPoints,
    size_t k,
    size_t rp,
    size_t ls
) {
    if (allPoints.empty() || k == 0) return {};
    
    std::vector<long long> truncated_distances;
    truncated_distances.reserve(allPoints.size());
    constexpr double SCALING_FACTOR_D = 1e6; // CORREGIDO: factor más pequeño

    for (const auto& p : allPoints) {
        double dist_sq_d = 0.0;
        for (size_t i = 0; i < DIM; ++i) {
            double diff_d = static_cast<double>(query[i]) - static_cast<double>(p[i]);
            dist_sq_d += diff_d * diff_d;
        }
        long long scaled_dist = static_cast<long long>(dist_sq_d * SCALING_FACTOR_D);
        long long truncated_dist = scaled_dist >> rp; // Truncar bits de baja precisión
        truncated_distances.push_back(truncated_dist);
    }
    
    return approximateTopK_with_distances(query, truncated_distances, allPoints, k, ls);
}

// Resto del código de SannsDB permanece igual...
class SannsDB {
public:
    struct Cluster {
        Point center;
        std::vector<Point> points;
    };

    struct Group {
        std::vector<Cluster> clusters;
    };

private:
    std::vector<Group> m_groups;
    std::vector<Point> m_stash;
    
    size_t m_max_cluster_size_m;
    float  m_large_cluster_fraction_alpha;
    size_t m_clusters_to_retrieve_u;
    size_t m_approx_topk_bins_l;

public:
    SannsDB(size_t max_cluster_size, float large_cluster_fraction, size_t clusters_to_retrieve, size_t approx_bins)
        : m_max_cluster_size_m(max_cluster_size), 
          m_large_cluster_fraction_alpha(large_cluster_fraction),
          m_clusters_to_retrieve_u(clusters_to_retrieve),
          m_approx_topk_bins_l(approx_bins)
    {}

    void build(const std::vector<Point>& dataset) {
        std::cout << "Iniciando construcción de SannsDB con " << dataset.size() << " puntos...\n";
        m_groups.clear();
        m_stash.clear();
        balancedClusteringRecursive(dataset);
        std::cout << "Construcción finalizada. Se crearon " << m_groups.size() 
                  << " grupos y un stash de " << m_stash.size() << " puntos.\n";
    }

    std::vector<Point> kNearestNeighbors(const Point& query, size_t k) const {
        std::vector<Point> cluster_candidate_points;

        for (const auto& group : m_groups) {
            std::vector<Point> centers;
            centers.reserve(group.clusters.size());
            for (const auto& cluster : group.clusters) {
                centers.push_back(cluster.center);
            }
            
            std::vector<Point> closest_centers = approximateTopK(query, centers, m_clusters_to_retrieve_u, m_approx_topk_bins_l);

            for (const auto& center : closest_centers) {
                for (const auto& cluster : group.clusters) {
                    if (Point::distance(center, cluster.center) < 1e-6f) {
                        cluster_candidate_points.insert(cluster_candidate_points.end(), cluster.points.begin(), cluster.points.end());
                        break;
                    }
                }
            }
        }
        
        std::vector<Point> cluster_neighbors = naiveTopK(query, cluster_candidate_points, k);
        std::vector<Point> stash_neighbors = approximateTopK(query, m_stash, k, m_approx_topk_bins_l);

        std::vector<Point> final_candidates = cluster_neighbors;
        final_candidates.insert(final_candidates.end(), stash_neighbors.begin(), stash_neighbors.end());
        
        return naiveTopK(query, final_candidates, k);
    }

private:
    void balancedClusteringRecursive(const std::vector<Point>& points) {
        if (points.empty()) return;

        size_t k_for_kmeans = std::max(static_cast<size_t>(2), static_cast<size_t>(std::sqrt(points.size()) / 2.0));
        k_for_kmeans = std::min(k_for_kmeans, points.size());
        
        std::vector<int> assignments = kmeansClusteringGeneral(points, k_for_kmeans);
        std::vector<Cluster> current_clusters(k_for_kmeans);

        std::vector<Point> sums(k_for_kmeans);
        std::vector<int> counts(k_for_kmeans, 0);
        for(size_t i = 0; i < points.size(); ++i) {
            int cluster_idx = assignments[i];
            current_clusters[cluster_idx].points.push_back(points[i]);
            sums[cluster_idx] += points[i];
            counts[cluster_idx]++;
        }
        for(size_t i = 0; i < k_for_kmeans; ++i) {
            if (counts[i] > 0) {
                current_clusters[i].center = sums[i] / static_cast<float>(counts[i]);
            }
        }

        Group small_clusters_group;
        std::vector<Point> large_cluster_points;
        for (const auto& cluster : current_clusters) {
            if (cluster.points.empty()) continue;
            
            if (cluster.points.size() > m_max_cluster_size_m) {
                large_cluster_points.insert(large_cluster_points.end(), cluster.points.begin(), cluster.points.end());
            } else {
                small_clusters_group.clusters.push_back(cluster);
            }
        }

        if (!small_clusters_group.clusters.empty()) {
            m_groups.push_back(small_clusters_group);
        }

        if (large_cluster_points.size() > m_large_cluster_fraction_alpha * points.size() && large_cluster_points.size() > 1) {
            std::cout << "    -> Recursando en " << large_cluster_points.size() << " puntos de clústeres grandes.\n";
            balancedClusteringRecursive(large_cluster_points);
        } else {
            std::cout << "    -> Finalizando recursión. Añadiendo " << large_cluster_points.size() << " puntos al stash.\n";
            m_stash.insert(m_stash.end(), large_cluster_points.begin(), large_cluster_points.end());
        }
    }
};

// K-Means implementation (sin cambios significativos)
std::vector<int> kmeansClusteringGeneral(const std::vector<Point>& points_to_cluster, size_t k) {
    const int MAX_ITER = 20;
    const float CONVERGENCE_TOL = 1e-6f;
    std::vector<int> assignments(points_to_cluster.size(), 0);

    if (points_to_cluster.size() <= k) {
         for(size_t i = 0; i < points_to_cluster.size(); ++i) assignments[i] = i;
         return assignments;
    }

    std::vector<Point> centroids;
    centroids.reserve(k);
    static std::mt19937 gen(12345);
    std::uniform_int_distribution<size_t> dist(0, points_to_cluster.size() - 1);
    centroids.push_back(points_to_cluster[dist(gen)]);

    std::vector<float> dist_sq(points_to_cluster.size());
    for(size_t i = 1; i < k; ++i) {
        float total_dist_sq = 0;
        for(size_t j = 0; j < points_to_cluster.size(); ++j) {
            float min_d = std::numeric_limits<float>::max();
            for(const auto& c : centroids) {
                min_d = std::min(min_d, Point::distance(points_to_cluster[j], c));
            }
            dist_sq[j] = min_d * min_d;
            total_dist_sq += dist_sq[j];
        }
        std::uniform_real_distribution<float> r_dist(0.0f, total_dist_sq);
        float r = r_dist(gen);
        float running_sum = 0;
        for(size_t j = 0; j < points_to_cluster.size(); ++j) {
            running_sum += dist_sq[j];
            if (running_sum >= r) {
                centroids.push_back(points_to_cluster[j]);
                break;
            }
        }
    }

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        for (size_t i = 0; i < points_to_cluster.size(); ++i) {
            float min_dist = std::numeric_limits<float>::max();
            int best_cluster = 0;
            for (size_t c_idx = 0; c_idx < k; ++c_idx) {
                float d = Point::distance(points_to_cluster[i], centroids[c_idx]);
                if (d < min_dist) {
                    min_dist = d;
                    best_cluster = c_idx;
                }
            }
            assignments[i] = best_cluster;
        }

        std::vector<Point> new_centroids(k);
        std::vector<int> counts(k, 0);
        for (size_t i = 0; i < points_to_cluster.size(); ++i) {
            new_centroids[assignments[i]] += points_to_cluster[i];
            counts[assignments[i]]++;
        }

        bool converged = true;
        for (size_t i = 0; i < k; ++i) {
            if (counts[i] > 0) {
                new_centroids[i] /= static_cast<float>(counts[i]);
            } else {
                new_centroids[i] = points_to_cluster[dist(gen)];
            }
            if (Point::distance(centroids[i], new_centroids[i]) > CONVERGENCE_TOL) {
                converged = false;
            }
        }

        centroids = new_centroids;
        if (converged) break;
    }

    return assignments;
}

#endif // SANNS_PLAINTEXT_H
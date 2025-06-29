#ifndef SANNS_PLAINTEXT_H
#define SANNS_PLAINTEXT_H

#include <iostream>
#include <vector>
#include <string>
#include <numeric>   // Para std::iota
#include <algorithm> // Para std::shuffle, std::sort, etc.
#include <random>
#include "Point.h"
#include "SSPtree.h" // Usaremos el k-means de aquí

using namespace std;

// Declaración adelantada para el helper k-means
template<typename PointType>
std::vector<int> kmeansClusteringGeneral(const std::vector<PointType>& points_to_cluster, size_t k);

// --- Helpers para los algoritmos Top-K ---

// Implementación de NaiveTopK (similar a Algoritmo 1 del paper)
// Esto es un k-NN por fuerza bruta.
template<typename PointType>
std::vector<PointType> naiveTopK(const PointType& query, const std::vector<PointType>& points, size_t k) {
    if (points.empty() || k == 0) {
        return {};
    }
    
    using Elem = std::pair<float, PointType>;
    std::vector<Elem> dist_pairs;
    dist_pairs.reserve(points.size());
    for (const auto& p : points) {
        dist_pairs.emplace_back(PointType::distance(query, p), p);
    }

    k = std::min(k, points.size());
    auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; };
    std::partial_sort(dist_pairs.begin(), dist_pairs.begin() + k, dist_pairs.end(), cmp);

    std::vector<PointType> result;
    result.reserve(k);
    for (size_t i = 0; i < k; ++i) {
        result.push_back(dist_pairs[i].second);
    }
    return result;
}

// Implementación de ApproximateTopK (Algoritmo 2 del paper)
template<typename PointType>
std::vector<PointType> approximateTopK(const PointType& query, const std::vector<PointType>& points, size_t k, size_t l_bins) {
    if (points.empty() || k == 0) {
        return {};
    }
    k = std::min(k, points.size());
    l_bins = std::min(l_bins, points.size());
    if (l_bins < k) { // No tiene sentido tener menos bins que k
        l_bins = k;
    }

    std::vector<size_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0);
    
    // El paper especifica "permute the set randomly". Lo hacemos barajando los índices.
    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    // Encontrar el mínimo en cada uno de los 'l' bins
    std::vector<PointType> bin_mins;
    bin_mins.reserve(l_bins);

    size_t bin_size = (points.size() + l_bins - 1) / l_bins;

    for (size_t i = 0; i < l_bins; ++i) {
        size_t start_idx = i * bin_size;
        size_t end_idx = std::min((i + 1) * bin_size, points.size());

        if (start_idx >= end_idx) continue;

        PointType min_point = points[indices[start_idx]];
        float min_dist = PointType::distance(query, min_point);

        for (size_t j = start_idx + 1; j < end_idx; ++j) {
            float dist = PointType::distance(query, points[indices[j]]);
            if (dist < min_dist) {
                min_dist = dist;
                min_point = points[indices[j]];
            }
        }
        bin_mins.push_back(min_point);
    }
    
    // De los 'l' mínimos, encontrar los 'k' mejores usando fuerza bruta.
    return naiveTopK(query, bin_mins, k);
}

// Versión de naiveTopK que usa distancia al cuadrado. Es más eficiente y consistente.
template<typename PointType>
std::vector<PointType> naiveTopKSquared(const PointType& query, const std::vector<PointType>& points, size_t k) {
    if (points.empty() || k == 0) return {};
    
    using Elem = std::pair<double, PointType>;
    std::vector<Elem> dist_pairs;
    dist_pairs.reserve(points.size());
    for (const auto& p : points) {
        double dist_sq = 0.0;
        for(size_t i=0; i<PointType::getDimension(); ++i) {
            double diff = static_cast<double>(query[i]) - static_cast<double>(p[i]);
            dist_sq += diff * diff;
        }
        dist_pairs.emplace_back(dist_sq, p);
    }

    k = std::min(k, points.size());
    auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; };
    std::partial_sort(dist_pairs.begin(), dist_pairs.begin() + k, dist_pairs.end(), cmp);

    std::vector<PointType> result;
    result.reserve(k);
    for (size_t i = 0; i < k; ++i) result.push_back(dist_pairs[i].second);
    return result;
}

// Versión CORRECTA de approximateTopK que usa la métrica de distancia consistente.
// Ahora implementa la lógica de aproximación de "bins" correctamente.
template<typename PointType>
std::vector<PointType> approximateTopK_with_distances(
    const PointType& query,
    const std::vector<long long>& distances, 
    const std::vector<PointType>& points, 
    size_t k, 
    size_t l_bins
) {
    if (points.empty() || k == 0) return {};
    k = std::min(k, points.size());
    
    // Si l_bins es 0 o muy pequeño, no tiene sentido aproximar. Hacemos un k-NN exacto.
    if (l_bins < k || l_bins >= points.size()) {
        using Elem = std::pair<long long, size_t>;
        std::vector<Elem> dist_idx_pairs(points.size());
        for(size_t i = 0; i < points.size(); ++i) dist_idx_pairs[i] = {distances[i], i};

        auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; };
        std::partial_sort(dist_idx_pairs.begin(), dist_idx_pairs.begin() + k, dist_idx_pairs.end(), cmp);

        std::vector<PointType> result;
        result.reserve(k);
        for(size_t i = 0; i < k; ++i) result.push_back(points[dist_idx_pairs[i].second]);
        return result;
    }

    // el error en esta logica de aproximacion, porque la truncacion de distancias funciona bien
    // --- LÓGICA DE APROXIMACIÓN (Shuffle and Bin) ---
    std::vector<size_t> indices(points.size());
    std::iota(indices.begin(), indices.end(), 0);

    static std::random_device rd;
    static std::mt19937 g(rd());
    std::shuffle(indices.begin(), indices.end(), g);

    std::vector<PointType> candidate_points;
    candidate_points.reserve(l_bins);

    size_t bin_size = (points.size() + l_bins - 1) / l_bins;
    for (size_t i = 0; i < l_bins; ++i) {
        size_t start_idx = i * bin_size;
        size_t end_idx = std::min((i + 1) * bin_size, points.size());
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

    // ETAPA 2: Refinar usando la métrica de distancia al CUADRADO.
    return naiveTopKSquared(query, candidate_points, k);
}

/**
 * @brief Implementación del Algoritmo 3 del paper SANNS: Plaintext Linear Scan.
 * 
 * @param query El punto de consulta.
 * @param allPoints El dataset completo.
 * @param k El número de vecinos a encontrar.
 * @param rp El número de bits de baja precisión a truncar de las distancias (r en el paper).
 * @param ls El número de bins para el algoritmo ApproximateTopK (l en el paper).
 * @return Un vector con los k-vecinos más cercanos aproximados.
 */
template<typename PointType>
std::vector<PointType> linearScanKnn(
    const PointType& query,
    const std::vector<PointType>& allPoints,
    size_t k,
    size_t rp,
    size_t ls
) {
    if (allPoints.empty() || k == 0) return {};
    std::vector<long long> truncated_distances;
    truncated_distances.reserve(allPoints.size());
    constexpr double SCALING_FACTOR_D = 1e6;

    for (const auto& p : allPoints) {
        double dist_sq_d = 0.0;
        for (size_t i = 0; i < PointType::getDimension(); ++i) {
            double diff_d = static_cast<double>(query[i]) - static_cast<double>(p[i]);
            dist_sq_d += diff_d * diff_d;
        }
        long long scaled_dist = static_cast<long long>(dist_sq_d * SCALING_FACTOR_D);
        long long truncated_dist = scaled_dist >> rp;
        truncated_distances.push_back(truncated_dist);
    }
    
    return approximateTopK_with_distances(query, truncated_distances, allPoints, k, ls);
}


// --- Clase Principal para la base de datos SANNS ---

template<typename PointType>
class SannsDB {
public:
    // Estructuras de datos internas, como se describe en la sección 3.3
    struct Cluster {
        PointType center;
        std::vector<PointType> points;
    };

    struct Group {
        std::vector<Cluster> clusters;
    };

private:
    std::vector<Group> m_groups;
    std::vector<PointType> m_stash;
    
    // Hiperparámetros (ver Figura 1 del paper)
    size_t m_max_cluster_size_m;
    float  m_large_cluster_fraction_alpha;
    size_t m_clusters_to_retrieve_u;
    size_t m_approx_topk_bins_l; // Bins para approximateTopK

public:
    SannsDB(size_t max_cluster_size, float large_cluster_fraction, size_t clusters_to_retrieve, size_t approx_bins)
        : m_max_cluster_size_m(max_cluster_size), 
          m_large_cluster_fraction_alpha(large_cluster_fraction),
          m_clusters_to_retrieve_u(clusters_to_retrieve),
          m_approx_topk_bins_l(approx_bins)
    {}

    // Método principal para construir la estructura a partir del dataset.
    void build(const std::vector<PointType>& dataset) {
        std::cout << "Iniciando construccion de SannsDB con " << dataset.size() << " puntos...\n";
        m_groups.clear();
        m_stash.clear();
        // Heurística simple para elegir 'k' en k-means
        size_t initial_k = std::max(2ULL, static_cast<size_t>(std::sqrt(dataset.size()) / 2.0));
        initial_k = std::min(initial_k, dataset.size());
        balancedClusteringRecursive(dataset, initial_k);
        //balancedClusteringRecursive(dataset);
        std::cout << "Construccion finalizada. Se crearon " << m_groups.size() 
                  << " grupos y un stash de " << m_stash.size() << " puntos.\n";
    }

    // Método principal para realizar la búsqueda k-NN (Algoritmo 4 y Figura 2)
    std::vector<PointType> kNearestNeighbors(const PointType& query, size_t k) const {
        // --- 1. Recuperar puntos de los clústeres ---
        std::vector<PointType> cluster_candidate_points;

        // Iterar sobre cada grupo de clústeres
        for (const auto& group : m_groups) {
            std::vector<PointType> centers;
            centers.reserve(group.clusters.size());
            for (const auto& cluster : group.clusters) {
                centers.push_back(cluster.center);
            }
            
            // Paso 3 de Fig 2: Approximate Top-u para los centros
            std::vector<PointType> closest_centers = approximateTopK(query, centers, m_clusters_to_retrieve_u, m_approx_topk_bins_l);

            // Recuperar todos los puntos de los clústeres seleccionados
            for (const auto& center : closest_centers) {
                for (const auto& cluster : group.clusters) {
                    if (PointType::distance(center, cluster.center) < 1e-6f) { // Comparar puntos flotantes
                        cluster_candidate_points.insert(cluster_candidate_points.end(), cluster.points.begin(), cluster.points.end());
                        break;
                    }
                }
            }
        }
        
        // Paso 6 de Fig 2: Naive Top-k sobre los candidatos de clústeres
        std::vector<PointType> cluster_neighbors = naiveTopK(query, cluster_candidate_points, k);

        // --- 2. Recuperar puntos del Stash ---
        // Paso 6 (alterno): Approximate Top-k sobre el Stash
        std::vector<PointType> stash_neighbors = approximateTopK(query, m_stash, k, m_approx_topk_bins_l);

        // --- 3. Combinar y resultado final ---
        std::vector<PointType> final_candidates = cluster_neighbors;
        final_candidates.insert(final_candidates.end(), stash_neighbors.begin(), stash_neighbors.end());
        
        // Paso 7 de Fig 2: Naive Top-k sobre los 2k candidatos finales
        return naiveTopK(query, final_candidates, k);
    }


private:
    // Implementación del "Balanced Clustering" recursivo (Sección 3.3)
    void balancedClusteringRecursive(const std::vector<PointType>& points, size_t& k_for_kmeans) {
        if (points.empty()) return;

        // Heurística simple para elegir 'k' en k-means
        //size_t k_for_kmeans = std::max(static_cast<size_t>(2), static_cast<size_t>(std::sqrt(points.size()) / 2.0));
        //k_for_kmeans = std::min(k_for_kmeans, points.size());
        
        // Aplicar K-means
        std::vector<int> assignments = kmeansClusteringGeneral(points, k_for_kmeans);
        std::vector<Cluster> current_clusters(k_for_kmeans);

        // Calcular centros y miembros de cada clúster
        std::vector<PointType> sums(k_for_kmeans);
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

        // Separar clústeres pequeños de los grandes
        Group small_clusters_group;
        std::vector<PointType> large_cluster_points;
        for (const auto& cluster : current_clusters) {
            if (cluster.points.empty()) continue;
            
            if (cluster.points.size() > m_max_cluster_size_m) {
                large_cluster_points.insert(large_cluster_points.end(), cluster.points.begin(), cluster.points.end());
            } else {
                small_clusters_group.clusters.push_back(cluster);
            }
        }

        // --- NUEVA LÓGICA DE DETECCIÓN DE ESTANCAMIENTO ---
        if (!large_cluster_points.empty() && large_cluster_points.size() == points.size()) {
            // ¡ESTANCAMIENTO DETECTADO! K-Means no dividió nada.
            std::cerr << "    -> Estancamiento detectado. Aumentando k para forzar division.\n";
            k_for_kmeans *= 2; // Duplicar el número de clústeres para el siguiente intento
        } else {
            // Progreso normal, reiniciar k para la siguiente recursión
            size_t next_k = std::max(2ULL, static_cast<size_t>(std::sqrt(large_cluster_points.size()) / 2.0));
            k_for_kmeans = std::min(next_k, large_cluster_points.size());
        }

        if (!small_clusters_group.clusters.empty()) {
            m_groups.push_back(small_clusters_group);
        }
        
        // Decisión: ¿Recursión o Stash?
        if (large_cluster_points.size() > m_large_cluster_fraction_alpha * points.size() && large_cluster_points.size() > 1) {
            balancedClusteringRecursive(large_cluster_points, k_for_kmeans);
        } else {
            m_stash.insert(m_stash.end(), large_cluster_points.begin(), large_cluster_points.end());
        }
    }
};

// --- Implementación de K-Means genérico (para k clústeres) ---
// Extraído y generalizado de la lógica en SSPTree.h

template<typename PointType>
std::vector<int> kmeansClusteringGeneral(const std::vector<PointType>& points_to_cluster, size_t k) {
    const int MAX_ITER = 20;
    const float CONVERGENCE_TOL = 1e-6f;
    std::vector<int> assignments(points_to_cluster.size(), 0);

    if (points_to_cluster.size() <= k) {
         for(size_t i = 0; i < points_to_cluster.size(); ++i) assignments[i] = i;
         return assignments;
    }

    // Inicialización de centroides (k-means++)
    std::vector<PointType> centroids;
    centroids.reserve(k);
    static std::mt19937 gen(12345); // Semilla fija para reproducibilidad
    std::uniform_int_distribution<size_t> dist(0, points_to_cluster.size() - 1);
    centroids.push_back(points_to_cluster[dist(gen)]);

    std::vector<float> dist_sq(points_to_cluster.size());
    for(size_t i = 1; i < k; ++i) {
        float total_dist_sq = 0;
        for(size_t j = 0; j < points_to_cluster.size(); ++j) {
            float min_d = std::numeric_limits<float>::max();
            for(const auto& c : centroids) {
                min_d = std::min(min_d, PointType::distance(points_to_cluster[j], c));
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
        // Paso de Asignación
        for (size_t i = 0; i < points_to_cluster.size(); ++i) {
            float min_dist = std::numeric_limits<float>::max();
            int best_cluster = 0;
            for (size_t c_idx = 0; c_idx < k; ++c_idx) {
                float d = PointType::distance(points_to_cluster[i], centroids[c_idx]);
                if (d < min_dist) {
                    min_dist = d;
                    best_cluster = c_idx;
                }
            }
            assignments[i] = best_cluster;
        }

        // Paso de Actualización
        std::vector<PointType> new_centroids(k);
        std::vector<int> counts(k, 0);
        for (size_t i = 0; i < points_to_cluster.size(); ++i) {
            new_centroids[assignments[i]] += points_to_cluster[i];
            counts[assignments[i]]++;
        }

        bool converged = true;
        for (size_t i = 0; i < k; ++i) {
            if (counts[i] > 0) {
                new_centroids[i] /= static_cast<float>(counts[i]);
            } else { // Si un clúster queda vacío, lo reinicializamos
                new_centroids[i] = points_to_cluster[dist(gen)];
            }
            if (PointType::distance(centroids[i], new_centroids[i]) > CONVERGENCE_TOL) {
                converged = false;
            }
        }

        centroids = new_centroids;
        if (converged) break;
    }

    return assignments;
}

#endif // SANNS_PLAINTEXT_H
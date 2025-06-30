#include <iostream>
#include <vector>
#include <string>
#include <iomanip>
#include <map>

// ... Incluir todos los headers necesarios ...
#include "../Data/SIFTReader.hpp"
#include "SANNS.h"
#include "SSPTree.h"

#include <chrono>

static constexpr float FLOAT_TOL = 1e-6f;

class Timer {
private:
    std::chrono::time_point<std::chrono::high_resolution_clock> start_time;

public:
    void start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    // Devuelve la duración en milisegundos (double)
    double stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> duration = end_time - start_time;
        return duration.count();
    }
};

// --- Funciones de Ayuda para los Tests (templated) ---
template<typename PointType>
bool equalPoint(const PointType& a, const PointType& b) { 
    return PointType::distance(a, b) < FLOAT_TOL; 
}

template<typename PointType>
bool pointInSphere(const PointType& p, const Sphere<PointType>& s) { 
    return PointType::distance(p, s.center) <= s.radius + FLOAT_TOL; 
}

template<typename PointType>
bool samePointSet(const std::vector<PointType>& A, const std::vector<PointType>& B) { 
    return true; 
}

template<typename PointType>
bool sameDistanceList(std::vector<float>& A, std::vector<float>& B) { 
    return true; 
}

// Estructura para almacenar los resultados del benchmark
struct BenchmarkResult {
    int dataset_size;
    double avg_time_ms;
    float avg_accuracy;
};

// --- Modificamos las funciones de test para que devuelvan BenchmarkResult ---

template<typename PointType>
BenchmarkResult testSannsClusteringKnn(const std::vector<PointType>& allPoints) {
    Timer timer;
    double total_build_time = 0;
    /*
    // --- Medición del Pre-cómputo ---
    constexpr size_t MAX_CLUSTER_SIZE_M = 500; //50
    constexpr size_t CLUSTERS_TO_RETRIEVE_U = 50; //5
    constexpr size_t APPROX_BINS_L_CLUSTERING = 500; //50
    */

    auto N = allPoints.size();
   
    // 0.05 es un valor estándar y robusto. Significa: "si los puntos problemáticos
    // son menos del 5% del total, no vale la pena recursar, muévelos al stash".
    constexpr float  LARGE_CLUSTER_FRAC_ALPHA = 0.05f; //0.05

    // Constante de ajuste. Un valor entre 2 y 5 suele funcionar bien.
    // Empecemos con 4.0 para un crecimiento moderado.
    const double M_CONSTANT = 4.0; 
    size_t MAX_CLUSTER_SIZE_M = static_cast<size_t>(M_CONSTANT * std::sqrt(N));
    // Añadir límites para evitar valores absurdos en los extremos.
    MAX_CLUSTER_SIZE_M = std::max(static_cast<size_t>(3 * 10), MAX_CLUSTER_SIZE_M); // Mínimo de 30, k=10
    MAX_CLUSTER_SIZE_M = std::min(static_cast<size_t>(2000), MAX_CLUSTER_SIZE_M);  // Máximo de 2000

    // Constante de ajuste. Un valor entre 1.0 y 2.0 es un buen punto de partida.
    const double U_CONSTANT = 1.5; 
    size_t CLUSTERS_TO_RETRIEVE_U = static_cast<size_t>(U_CONSTANT * std::log2(N));
    // Añadir límites.
    CLUSTERS_TO_RETRIEVE_U = std::max(static_cast<size_t>(3), CLUSTERS_TO_RETRIEVE_U); // Mínimo de 3 clústeres
    CLUSTERS_TO_RETRIEVE_U = std::min(static_cast<size_t>(500), CLUSTERS_TO_RETRIEVE_U); // Máximo de 50

    // Queremos que L sea significativamente mayor que U para una buena aproximación.
    // Un factor de 5x a 10x es razonable.
    const double L_FACTOR = 8.0; 
    size_t APPROX_BINS_L_CLUSTERING = static_cast<size_t>(L_FACTOR * CLUSTERS_TO_RETRIEVE_U);


    timer.start();
    SannsDB<PointType> sanns_db(MAX_CLUSTER_SIZE_M, LARGE_CLUSTER_FRAC_ALPHA, CLUSTERS_TO_RETRIEVE_U, APPROX_BINS_L_CLUSTERING);
    sanns_db.build(allPoints);
    total_build_time = timer.stop();
    std::cout << "    Build time: " << total_build_time << " ms\n";

    // --- Medición de la Búsqueda ---
    float total_accuracy = 0.0f;
    double total_query_time = 0;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);

    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)];
        
        timer.start();
        std::vector<PointType> sanns_res = sanns_db.kNearestNeighbors(query, K);
        total_query_time += timer.stop();
        
        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);
        int correct_found = 0;
        for (const auto& s_p : sanns_res) for (const auto& b_p : brute_res) if (equalPoint(s_p, b_p)) { correct_found++; break; }
        total_accuracy += static_cast<float>(correct_found) / K;
    }
    
    return {
        (int)allPoints.size(),
        total_query_time / NUM_TESTS,
        total_accuracy / NUM_TESTS
    };
}

template<typename PointType>
BenchmarkResult testSannsLinearScan(const std::vector<PointType>& allPoints) {
    float total_accuracy = 0.0f;
    double total_query_time = 0;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;
    constexpr size_t RP_BITS = 8;
    constexpr size_t LS_BINS = 200;

    Timer timer;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);

    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)];
        
        timer.start();
        std::vector<PointType> sanns_res = linearScanKnn(query, allPoints, K, RP_BITS, LS_BINS);
        total_query_time += timer.stop();

        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);
        int correct_found = 0;
        for (const auto& s_p : sanns_res) for (const auto& b_p : brute_res) if (equalPoint(s_p, b_p)) { correct_found++; break; }
        total_accuracy += static_cast<float>(correct_found) / K;
    }
    
    return {
        (int)allPoints.size(),
        total_query_time / NUM_TESTS,
        total_accuracy / NUM_TESTS
    };
}

template<typename PointType>
BenchmarkResult testSspTreeExperimentalKnn(const std::vector<PointType>& allPoints) {
    Timer timer;
    double total_build_time = 0;
    /*
    constexpr size_t MAX_ENTRIES = 32;
    constexpr size_t SEARCH_BUDGET = 256;
    */

    auto N = allPoints.size();
    // Constante de ajuste. Un valor entre 8 y 12 puede funcionar bien.
    const double ME_CONSTANT = 10.0;
    size_t MAX_ENTRIES = static_cast<size_t>(ME_CONSTANT * std::log10(N));
    // Añadir límites para que no sea ni demasiado pequeño ni demasiado grande.
    // Un árbol binario (2) es el mínimo, pero poco eficiente. Empecemos en 8.
    MAX_ENTRIES = std::max(static_cast<size_t>(8), MAX_ENTRIES);
    // Un fan-out demasiado grande (> 64) puede ralentizar el descenso.
    MAX_ENTRIES = std::min(static_cast<size_t>(64), MAX_ENTRIES);

    // Constante de ajuste. Este valor controla directamente el "esfuerzo" de la búsqueda.
    // Un valor entre 5 y 10 es un buen punto de partida.
    const double SB_CONSTANT = 8.0; 
    size_t SEARCH_BUDGET = static_cast<size_t>(SB_CONSTANT * std::sqrt(N));

    // Añadir límites.
    // Un presupuesto mínimo para asegurar que se explore más allá de la raíz y sus hijos.
    SEARCH_BUDGET = std::max(static_cast<size_t>(64), SEARCH_BUDGET);
    // Un presupuesto máximo para evitar que las consultas se vuelvan demasiado lentas en datasets gigantes.
    SEARCH_BUDGET = std::min(static_cast<size_t>(4096), SEARCH_BUDGET);


    timer.start();
    SSPTree<PointType> sift_tree(MAX_ENTRIES);
    for(const auto& vec : allPoints) sift_tree.insert(vec);
    total_build_time = timer.stop();
    std::cout << "    Build time: " << total_build_time << " ms\n";

    float total_accuracy = 0.0f;
    double total_query_time = 0;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);

    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)];
        
        timer.start();
        std::vector<PointType> experimental_res = sift_tree.experimentalKnn(query, K, SEARCH_BUDGET);
        total_query_time += timer.stop();

        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);
        int correct_found = 0;
        for (const auto& e_p : experimental_res) for (const auto& b_p : brute_res) if (equalPoint(e_p, b_p)) { correct_found++; break; }
        total_accuracy += static_cast<float>(correct_found) / K;
    }

    return {
        (int)allPoints.size(),
        total_query_time / NUM_TESTS,
        total_accuracy / NUM_TESTS
    };
}

void print_results_table(const std::map<std::string, std::vector<BenchmarkResult>>& all_results) {
    std::cout << "\n\n--- TABLA DE RESULTADOS DEL BENCHMARK ---\n\n";
    std::cout << std::left
              << std::setw(30) << "Algoritmo"
              << std::setw(15) << "Dataset Size"
              << std::setw(20) << "Avg Query Time (ms)"
              << std::setw(20) << "Accuracy (%)"
              << std::endl;
    std::cout << std::string(85, '-') << std::endl;

    for(const auto& pair : all_results) {
        const std::string& name = pair.first;
        const auto& results = pair.second;
        for(const auto& res : results) {
            std::cout << std::left
                      << std::setw(30) << name
                      << std::setw(15) << res.dataset_size
                      << std::fixed << std::setprecision(4) << std::setw(20) << res.avg_time_ms
                      << std::fixed << std::setprecision(2) << std::setw(20) << res.avg_accuracy * 100.0
                      << std::endl;
        }
    }
    std::cout << "\n";
}


int main() {
    // --- Cargar el dataset completo una vez ---
    std::cout << "=== CARGANDO DATASET SIFT COMPLETO ===\n";
    std::vector<SIFTVector> full_sift_dataset = SIFTReader::readFile("../Data/sift_base.fvecs");
    if (full_sift_dataset.empty()) {
        std::cerr << "Error: No se pudieron cargar los vectores SIFT\n";
        return 1;
    }
    std::cout << "Cargados " << full_sift_dataset.size() << " vectores SIFT.\n";

    // --- Definir los tamaños de dataset para el benchmark ---
    std::vector<int> dataset_sizes = {100000};
    
    // Mapa para almacenar todos los resultados
    std::map<std::string, std::vector<BenchmarkResult>> all_results;

    for (int size : dataset_sizes) {
        if (size > full_sift_dataset.size()) continue;

        std::cout << "\n" << std::string(80, '=') << "\n";
        std::cout << "INICIANDO BENCHMARK CON DATASET SIZE = " << size << "\n";
        std::cout << std::string(80, '=') << "\n\n";

        // Crear el subconjunto de datos para esta iteración
        std::vector<SIFTVector> current_dataset(full_sift_dataset.begin(), full_sift_dataset.begin() + size);

        // --- Ejecutar cada test ---
        std::cout << "--- Ejecutando Test: SANNS Clustering (Plaintext) ---\n";
        all_results["SANNS Clustering"].push_back(testSannsClusteringKnn<SIFTVector>(current_dataset));

        std::cout << "\n--- Ejecutando Test: SANNS Linear Scan (Plaintext) ---\n";
        all_results["SANNS Linear Scan"].push_back(testSannsLinearScan<SIFTVector>(current_dataset));

        std::cout << "\n--- Ejecutando Test: SSP-Tree Experimental ---\n";
        all_results["SSP-Tree Experimental"].push_back(testSspTreeExperimentalKnn<SIFTVector>(current_dataset));
    }

    // --- Imprimir la tabla final de resultados ---
    print_results_table(all_results);

    std::cout << "Benchmark finalizado.\n";
    return 0;
}
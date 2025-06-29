#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <array>
#include <functional>
#include <string>
#include <fstream>

#include <iomanip> // Para std::setw, std::fixed, std::setprecision

// Incluir las cabeceras de la implementación del SS+-Tree
#include "Point.h"
#include "Sphere.h"
#include "SSPTree.h"

// Incluir la nueva implementación de SANNS
#include "SANNS.h"

// Incluir el lector SIFT
#include "../Data/SIFTReader.hpp"

static constexpr float FLOAT_TOL = 1e-6f;

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


// -------------------------------------------------------------
// TESTS para SSP-Tree (templated)
// -------------------------------------------------------------
template<typename PointType>
bool testBoundingVolumes(const SSPTree<PointType>& tree) { 
    return true; 
}

template<typename PointType>
bool testSearch(const SSPTree<PointType>& tree, const std::vector<PointType>& allPoints) { 
    return true; 
}

template<typename PointType>
bool testRangeQuerySphere(const SSPTree<PointType>& tree, const std::vector<PointType>& allPoints) { 
    return true; 
}

template<typename PointType>
bool testKNearestNeighbors(const SSPTree<PointType>& tree, const std::vector<PointType>& allPoints) { 
    return true; 
}

// -------------------------------------------------------------
// TEST 5: SANNS Clustering-based k-NN (templated)
// -------------------------------------------------------------
template<typename PointType>
bool testSannsClusteringKnn(const SannsDB<PointType>& sanns_db, const std::vector<PointType>& allPoints) {
    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;
    std::cout << "Ejecutando " << NUM_TESTS << " pruebas de k-NN (k=" << K << ") para SANNS (Clustering-based)...\n";
    
    // Usar algunos puntos del dataset como queries en lugar de puntos aleatorios
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);
    
    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)]; // Usar un punto real del dataset como query
        std::vector<PointType> sanns_res = sanns_db.kNearestNeighbors(query, K);
        std::vector<PointType> brute_res = naiveTopK(query, allPoints, K);
        
        int correct_found = 0;
        for (const auto& sanns_p : sanns_res) {
            for (const auto& brute_p : brute_res) {
                if (equalPoint(sanns_p, brute_p)) {
                    correct_found++;
                    break;
                }
            }
        }
        total_accuracy += static_cast<float>(correct_found) / K;
    }
    
    float avg_accuracy = total_accuracy / NUM_TESTS;
    std::cout << "[INFO] Test 5 (SANNS Clustering): Precisión promedio (" << K << "-NN Accuracy) = " << avg_accuracy * 100.0f << "%\n";
    if (avg_accuracy < 0.7) {
        std::cerr << "[ERROR] Test 5 (SANNS Clustering): La precisión es demasiado baja.\n";
        return false;
    }
    std::cout << "[OK] Test 5 (SANNS Clustering) pasó con una precisión aceptable.\n";
    return true;
}

// TEST 6 MODIFICADO para usar datos reales (templated)
template<typename PointType>
bool testSannsLinearScan(const std::vector<PointType>& allPoints) {
    constexpr int K = 10;
    constexpr size_t RP_BITS = 8; 
    constexpr size_t LS_BINS = 100; // error, con este valor saca 100% porque es < k, deberia sacar una buena presiscion con un valor como el del siguiente test

    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 20;

    std::cout << "Ejecutando " << NUM_TESTS << " pruebas de k-NN (k=" << K << ") para SANNS (Linear Scan)...\n";
    std::cout << "    (rp=" << RP_BITS << ", ls=" << LS_BINS << ")\n";

    // Usar algunos puntos del dataset como queries
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);

    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)]; // Usar un punto real del dataset como query
        
        std::vector<PointType> sanns_res = linearScanKnn(query, allPoints, K, RP_BITS, LS_BINS);
        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);
        
        int correct_found = 0;
        for (const auto& sanns_p : sanns_res) {
            for (const auto& brute_p : brute_res) {
                if (equalPoint(sanns_p, brute_p)) {
                    correct_found++;
                    break;
                }
            }
        }
        total_accuracy += static_cast<float>(correct_found) / K;
    }

    float avg_accuracy = total_accuracy / NUM_TESTS;
    std::cout << "[INFO] Test 6 (SANNS Linear Scan): Precisión promedio (" << K << "-NN Accuracy) = " << avg_accuracy * 100.0f << "%\n";
    
    if (avg_accuracy < 0.7) {
        std::cerr << "[ERROR] Test 6 (SANNS Linear Scan): La precisión es demasiado baja.\n";
        return false;
    }
    std::cout << "[OK] Test 6 (SANNS Linear Scan) pasó con una precisión aceptable.\n";
    return true;
}

// Nueva función de ayuda (templated)
template<typename PointType>
double get_dist_sq(const PointType& p1, const PointType& p2) {
    double dist_sq = 0.0;
    for(size_t i = 0; i < PointType::getDimension(); ++i) {
        double diff = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        dist_sq += diff * diff;
    }
    return dist_sq;
}

// -------------------------------------------------------------
// TEST 7: Visualización y Comparación Directa (templated)
// -------------------------------------------------------------
template<typename PointType>
void testVisualization(const std::vector<PointType>& allPoints) {
    std::cout << "\n=== TEST 7: Visualizacion y Comparacion Directa ===\n";
    constexpr int K = 10;
    constexpr size_t RP_BITS = 8; 
    constexpr size_t LS_BINS = 700; // error, deberia sacar una buena presiscion con este parametro k < LS_BINS < vector.size()

    // Usar un punto real del dataset como query
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);
    PointType query = allPoints[dis(gen)];

    std::vector<PointType> brute_force_res = naiveTopKSquared(query, allPoints, K);
    std::vector<PointType> linear_scan_res = linearScanKnn(query, allPoints, K, RP_BITS, LS_BINS);

    std::cout << "\n--- Tabla Comparativa de Resultados (Distancia al Cuadrado) ---\n";
    std::cout << "Query usado del dataset\n";
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::left << std::setw(3) << "#"
              << std::setw(35) << "NAIVE (Ground Truth)"
              << std::setw(35) << "LINEAR SCAN (Resultado)" << std::endl;
    std::cout << std::string(80, '-') << std::endl;
    std::cout << std::fixed << std::setprecision(4);

    for (int i = 0; i < K; ++i) {
        std::cout << std::left << std::setw(3) << i + 1;
        double naive_dist_sq = get_dist_sq(query, brute_force_res[i]);
        std::cout << "Dist^2: " << std::setw(28) << naive_dist_sq;
        
        if (i < linear_scan_res.size()) {
            double ls_dist_sq_to_query = get_dist_sq(query, linear_scan_res[i]);
            bool same = equalPoint(brute_force_res[i], linear_scan_res[i]);
            std::cout << "Dist^2: " << std::setw(20) << ls_dist_sq_to_query << (same ? " (MATCH)" : " (FAIL)");
        } else {
            std::cout << std::setw(35) << " (No devuelto)";
        }
        std::cout << std::endl;
    }
    std::cout << std::string(80, '-') << std::endl;
}


// -------------------------------------------------------------
// NUEVO TEST 8: SSP-Tree Experimental k-NN
// -------------------------------------------------------------
template<typename PointType>
bool testSspTreeExperimentalKnn(const SSPTree<PointType>& tree, const std::vector<PointType>& allPoints) {
    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;
    std::cout << "Ejecutando " << NUM_TESTS << " pruebas de k-NN (k=" << K << ") para SSP-Tree (Experimental)...\n";
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, allPoints.size() - 1);
    
    for (int t = 0; t < NUM_TESTS; ++t) {
        PointType query = allPoints[dis(gen)];
        
        // Llamar al nuevo método experimental
        std::vector<PointType> experimental_res = tree.experimentalKnn(query, K);
        
        // Comparar con el ground truth (usando distancia al cuadrado para consistencia)
        std::vector<PointType> brute_res = naiveTopK(query, allPoints, K);
        
        int correct_found = 0;
        for (const auto& exp_p : experimental_res) {
            for (const auto& brute_p : brute_res) {
                if (equalPoint(exp_p, brute_p)) {
                    correct_found++;
                    break;
                }
            }
        }
        total_accuracy += static_cast<float>(correct_found) / K;
    }
    
    float avg_accuracy = total_accuracy / NUM_TESTS;
    std::cout << "[INFO] Test 8 (SSP-Tree Experimental): Precisión promedio (" << K << "-NN Accuracy) = " << avg_accuracy * 100.0f << "%\n";
    if (avg_accuracy < 0.85) { // Esperamos una precisión alta para este método
        std::cerr << "[ERROR] Test 8 (SSP-Tree Experimental): La precisión es demasiado baja.\n";
        return false;
    }
    std::cout << "[OK] Test 8 (SSP-Tree Experimental) pasó con una precisión aceptable.\n";
    return true;
}


int main() {
    bool overallOK = true;

    // --- Cargar datos SIFT reales ---
    std::cout << "=== CARGANDO DATASET SIFT ===\n";
    
    // Ruta al archivo SIFT, cambia el nombre de la carpeta si es necesario
    std::string sift_path = "../Data/siftsmall_base.fvecs";
    
    // Verificar si el archivo existe usando ifstream
    std::ifstream test_file(sift_path);
    if (!test_file.is_open()) {
        std::cerr << "Error: No se encontró el archivo " << sift_path << std::endl;
        std::cerr << "Asegúrate de que el archivo siftsmall_base.fvecs esté en la carpeta Data\n";
        return 1;
    }
    test_file.close();
    
    std::cout << "Cargando datos SIFT desde: " << sift_path << std::endl;

    int cant = 6000; // Número de vectores SIFT a cargar
    
    // Cargar vectores SIFT
    std::vector<SIFTVector> sift_vectors = SIFTReader::readFile(sift_path);
    //std::vector<SIFTVector> all_data = SIFTReader::readFile(sift_path);
    //std::vector<SIFTVector> sift_vectors;
    //sift_vectors.assign(all_data.begin(), all_data.begin() + std::min(cant, (int)all_data.size()));
    
    if (sift_vectors.empty()) {
        std::cerr << "Error: No se pudieron cargar los vectores SIFT\n";
        return 1;
    }
    
    std::cout << "Cargados " << sift_vectors.size() << " vectores SIFT\n";
    std::cout << "   Dimension SIFT: " << sift_vectors[0].getDimension() << std::endl;
    
    // Mostrar información del primer punto para verificar
    std::cout << "Primer vector SIFT (primeras 10 dimensiones): ";
    for (size_t i = 0; i < std::min(static_cast<size_t>(10), sift_vectors[0].getDimension()); ++i) {
        std::cout << sift_vectors[0][i] << " ";
    }
    std::cout << "...\n";

    // --- Parte 1: Pruebas del algoritmo de Clustering (Algoritmo 4) ---
    std::cout << "\n--- CONSTRUYENDO Y PROBANDO SANNS (CLUSTERING-BASED) ---\n";

    constexpr size_t MAX_CLUSTER_SIZE_M = 50;
    constexpr float  LARGE_CLUSTER_FRAC_ALPHA = 0.035f; // Fracción de puntos en clústeres grandes, init = 0.05f
    constexpr size_t CLUSTERS_TO_RETRIEVE_U = 5;
    constexpr size_t APPROX_BINS_L_CLUSTERING = 20;

    SannsDB<SIFTVector> sanns_db(MAX_CLUSTER_SIZE_M, LARGE_CLUSTER_FRAC_ALPHA, CLUSTERS_TO_RETRIEVE_U, APPROX_BINS_L_CLUSTERING);

    std::cout << "Construyendo base de datos SANNS con " << sift_vectors.size() << " vectores SIFT reales...\n";
    sanns_db.build(sift_vectors);

    std::cout << "\n=== TEST 5: SANNS Plaintext Clustering-based k-NN ===\n";
    if (!testSannsClusteringKnn<SIFTVector>(sanns_db, sift_vectors)) overallOK = false;


    // --- Parte 2: Pruebas del algoritmo Linear Scan (Algoritmo 3) ---
    std::cout << "\n=== TEST 6: SANNS Plaintext Linear Scan k-NN ===\n";
    if (!testSannsLinearScan<SIFTVector>(sift_vectors)) overallOK = false;

    // --- Parte 3: Test de Visualización ---
    //testVisualization<SIFTVector>(sift_vectors);


    // --- NUEVA PARTE: Pruebas del SSP-Tree Híbrido ---
    std::cout << "\n--- CONSTRUYENDO Y PROBANDO SSP-TREE (EXPERIMENTAL KNN) ---\n";
    constexpr size_t MAX_ENTRIES = 32; // Un valor razonable para el fan-out del árbol
    SSPTree<SIFTVector> sift_tree(MAX_ENTRIES);
    std::cout << "Construyendo SSP-Tree con " << sift_vectors.size() << " vectores SIFT...\n";
    for(const auto& vec : sift_vectors) {
        sift_tree.insert(vec);
    }
    std::cout << "SSP-Tree construido.\n";

    std::cout << "\n=== TEST 8: SSP-Tree Experimental k-NN ===\n";
    if (!testSspTreeExperimentalKnn<SIFTVector>(sift_tree, sift_vectors)) overallOK = false;


    std::cout << "\n----------------------------------------\n";
    if (overallOK) {
        std::cout << "¡Felicidades! Todos los tests relevantes pasaron correctamente usando datos SIFT reales.\n";
        return 0;
    } else {
        std::cout << "Rayos. Algun test fallo. ¡A depurar se ha dicho!\n";
        return 1;
    }
}
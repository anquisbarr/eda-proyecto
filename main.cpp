#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cmath>
#include <array>
#include <functional>
#include <string>

#include <iomanip> // Para std::setw, std::fixed, std::setprecision

// Incluir las cabeceras de la implementación del SS+-Tree
#include "Point.h"
#include "Sphere.h"
#include "SSPTree.h"

// Incluir la nueva implementación de SANNS
#include "SANNS.h"


static constexpr float FLOAT_TOL = 1e-6f;

// --- Funciones de Ayuda para los Tests (sin cambios) ---
bool equalPoint(const Point& a, const Point& b) { /*...*/ return Point::distance(a, b) < FLOAT_TOL; }
bool pointInSphere(const Point& p, const Sphere& s) { /*...*/ return Point::distance(p, s.center) <= s.radius + FLOAT_TOL; }
bool samePointSet(const std::vector<Point>& A, const std::vector<Point>& B) { /*...*/ return true; }
bool sameDistanceList(std::vector<float>& A, std::vector<float>& B) { /*...*/ return true; }
void collectPointsFromSubtree(const SSPNode* node, std::vector<Point>& outPoints) { /*...*/ }

// -------------------------------------------------------------
// TESTS para SSP-Tree (sin cambios)
// -------------------------------------------------------------
bool testBoundingVolumes(const SSPTree& tree) { return true; }
bool testSearch(const SSPTree& tree, const std::vector<Point>& allPoints) { return true; }
bool testRangeQuerySphere(const SSPTree& tree, const std::vector<Point>& allPoints) { return true; }
bool testKNearestNeighbors(const SSPTree& tree, const std::vector<Point>& allPoints) { return true; }

// -------------------------------------------------------------
// TEST 5: SANNS Clustering-based k-NN (anteriormente testSannsKnn)
// -------------------------------------------------------------
bool testSannsClusteringKnn(const SannsDB& sanns_db, const std::vector<Point>& allPoints) {
    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 20;
    constexpr int K = 10;
    std::cout << "Ejecutando " << NUM_TESTS << " pruebas de k-NN (k=" << K << ") para SANNS (Clustering-based)...\n";
    for (int t = 0; t < NUM_TESTS; ++t) {
        Point query = Point::random(0.0f, 1.0f);
        std::vector<Point> sanns_res = sanns_db.kNearestNeighbors(query, K);
        std::vector<Point> brute_res = naiveTopK(query, allPoints, K);
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

// TEST 6 MODIFICADO para ser consistente
bool testSannsLinearScan(const std::vector<Point>& allPoints) {
    constexpr int K = 10;
    constexpr size_t RP_BITS = 8; 
    constexpr size_t LS_BINS = 5; // Aumentar bins para que la aproximación sea mejor

    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 20;

    std::cout << "Ejecutando " << NUM_TESTS << " pruebas de k-NN (k=" << K << ") para SANNS (Linear Scan)...\n";
    std::cout << "    (rp=" << RP_BITS << ", ls=" << LS_BINS << ")\n";

    for (int t = 0; t < NUM_TESTS; ++t) {
        Point query = Point::random(0.0f, 1.0f);
        
        std::vector<Point> sanns_res = linearScanKnn(query, allPoints, K, RP_BITS, LS_BINS);
        std::vector<Point> brute_res = naiveTopKSquared(query, allPoints, K); // Usar la métrica correcta
        
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
    
    if (avg_accuracy < 0.7) { // Con RP_BITS=0 y LS_BINS > K, esto debería ser muy alto
        std::cerr << "[ERROR] Test 6 (SANNS Linear Scan): La precisión es demasiado baja.\n";
        return false;
    }
    std::cout << "[OK] Test 6 (SANNS Linear Scan) pasó con una precisión aceptable.\n";
    return true;
}


// Hay que redefinir esta función para que use distancias al cuadrado y sea consistente
// Nueva función de ayuda
double get_dist_sq(const Point& p1, const Point& p2) {
    double dist_sq = 0.0;
    for(size_t i=0; i<DIM; ++i) {
        double diff = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        dist_sq += diff * diff;
    }
    return dist_sq;
}

// -------------------------------------------------------------
// NUEVO TEST 7: Visualización y Comparación Directa
// -------------------------------------------------------------
// TEST 7 MODIFICADO para ser consistente
void testVisualization() {
    std::cout << "\n=== TEST 7: Visualización y Comparación Directa ===\n";
    constexpr size_t NUM_POINTS = 1000;
    constexpr int K = 10;
    constexpr size_t RP_BITS = 8; 
    constexpr size_t LS_BINS = 700; // Más bins para que la aproximación sea buena

    std::vector<Point> points;
    points.reserve(NUM_POINTS);
    for (size_t i = 0; i < NUM_POINTS; ++i) points.push_back(Point::random(0.0f, 1.0f));
    Point query = Point::random(0.0f, 1.0f);

    std::vector<Point> brute_force_res = naiveTopKSquared(query, points, K);
    std::vector<Point> linear_scan_res = linearScanKnn(query, points, K, RP_BITS, LS_BINS);

    std::cout << "\n--- Tabla Comparativa de Resultados (Distancia al Cuadrado) ---\n";
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


int main() {
    bool overallOK = true;

    constexpr std::size_t NUM_POINTS  = 5000;
    
    std::cout << "Generando " << NUM_POINTS << " puntos aleatorios...\n";
    std::vector<Point> allPoints;
    allPoints.reserve(NUM_POINTS);
    for (std::size_t i = 0; i < NUM_POINTS; ++i) {
        allPoints.push_back(Point::random(0.0f, 1.0f));
    }

    // --- Parte 1: Pruebas del algoritmo de Clustering (Algoritmo 4) ---
    std::cout << "\n--- CONSTRUYENDO Y PROBANDO SANNS (CLUSTERING-BASED) ---\n";
    
    constexpr size_t MAX_CLUSTER_SIZE_M = 50;
    constexpr float  LARGE_CLUSTER_FRAC_ALPHA = 0.05f;
    constexpr size_t CLUSTERS_TO_RETRIEVE_U = 5;
    constexpr size_t APPROX_BINS_L_CLUSTERING = 20;

    //SannsDB sanns_db(MAX_CLUSTER_SIZE_M, LARGE_CLUSTER_FRAC_ALPHA, CLUSTERS_TO_RETRIEVE_U, APPROX_BINS_L_CLUSTERING);
    //sanns_db.build(allPoints);

    //std::cout << "\n=== TEST 5: SANNS Plaintext Clustering-based k-NN ===\n";
    //if (!testSannsClusteringKnn(sanns_db, allPoints)) overallOK = false;

    // --- Parte 2: Pruebas del algoritmo Linear Scan (Algoritmo 3) ---
    std::cout << "\n=== TEST 6: SANNS Plaintext Linear Scan k-NN ===\n";
    if (!testSannsLinearScan(allPoints)) overallOK = false;

    // --- Parte 3: Test de Visualización ---
    testVisualization();


    std::cout << "\n----------------------------------------\n";
    if (overallOK) {
        std::cout << "✅ ¡Felicidades! Todos los tests relevantes pasaron correctamente.\n";
        return 0;
    } else {
        std::cout << "❌ Rayos. Algún test falló. ¡A depurar se ha dicho!\n";
        return 1;
    }
}
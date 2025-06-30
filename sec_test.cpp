#include <iostream>
// ... otros includes ...
#include "../Data/SIFTReader.hpp"
#include "CryptoPrimitives.h" // Incluir
#include "SecureSANNS.h"      // Incluir


// -------------------------------------------------------------
// NUEVO TEST 9: Secure Linear Scan k-NN (Simulado)
// -------------------------------------------------------------
template<typename PointType>
bool testSecureLinearScan(const std::vector<PointType>& allPoints) {
    std::cout << "\n=== TEST 9: Secure Linear Scan k-NN (Simulado) ===\n";

    constexpr int NUM_TESTS = 5;
    constexpr int K = 10;
    int successes = 0;

    for(int t = 0; t < NUM_TESTS; ++t) {
        // Usar un punto aleatorio del dataset como query
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, allPoints.size() - 1);
        PointType query = allPoints[dis(gen)];

        // Ejecutar el protocolo seguro simulado
        std::vector<PointType> secure_res = secureLinearScanKnn(query, allPoints, K);

        // Calcular el ground truth
        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);

        // Verificar si los resultados son idénticos
        bool sets_are_equal = true;
        if (secure_res.size() != brute_res.size()) {
            sets_are_equal = false;
        } else {
            for(size_t i = 0; i < secure_res.size(); ++i) {
                if (!equalPoint(secure_res[i], brute_res[i])) {
                    sets_are_equal = false;
                    break;
                }
            }
        }

        if(sets_are_equal) {
            successes++;
        }
    }

    std::cout << "[INFO] Test 9 (Secure Linear Scan): " << successes << " de " << NUM_TESTS << " pruebas fueron correctas.\n";
    if (successes == NUM_TESTS) {
        std::cout << "[OK] Test 9 (Secure Linear Scan) paso correctamente.\n";
        return true;
    } else {
        std::cerr << "[ERROR] Test 9 (Secure Linear Scan) fallo.\n";
        return false;
    }
}

// -------------------------------------------------------------
// NUEVO TEST 10: Secure Clustering k-NN (Simulado)
// -------------------------------------------------------------
template<typename PointType>
bool testSecureClusteringKnn(const std::vector<PointType>& allPoints) {
    std::cout << "\n=== TEST 10: Secure Clustering k-NN (Simulado) ===\n";
    
    /*
    std::vector<PointType> allPoints = SIFTReader::readFile("../Data/siftsmall_base.fvecs", 2000);
    if(allPoints.size() < 2000) {
        std::cerr << "No se cargaron suficientes puntos para el test.\n";
        return false;
    }
    */
    
    // --- Setup del Servidor (pre-cómputo) ---
    constexpr size_t MAX_CLUSTER_SIZE_M = 50;
    constexpr float  LARGE_CLUSTER_FRAC_ALPHA = 0.05f;
    constexpr size_t CLUSTERS_TO_RETRIEVE_U = 5;
    constexpr size_t APPROX_BINS_L_CLUSTERING = 50;
    
    std::cout << "Servidor pre-calculando la estructura de clustering...\n";
    SecureServer<PointType> server(allPoints, MAX_CLUSTER_SIZE_M, LARGE_CLUSTER_FRAC_ALPHA, CLUSTERS_TO_RETRIEVE_U, APPROX_BINS_L_CLUSTERING);
    std::cout << "Estructura del servidor lista.\n";

    // --- Ejecución de Tests ---
    float total_accuracy = 0.0f;
    constexpr int NUM_TESTS = 10;
    constexpr int K = 10;

    for (int t = 0; t < NUM_TESTS; ++t) {
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<> dis(0, allPoints.size() - 1);
        PointType query = allPoints[dis(gen)];

        // Ejecutar el protocolo seguro de clustering
        std::vector<PointType> secure_res = secureClusteringKnn(query, server, K);
        
        // Calcular ground truth para comparar
        std::vector<PointType> brute_res = naiveTopKSquared(query, allPoints, K);

        int correct_found = 0;
        for (const auto& sec_p : secure_res) {
            for (const auto& brute_p : brute_res) {
                if (equalPoint(sec_p, brute_p)) {
                    correct_found++;
                    break;
                }
            }
        }
        total_accuracy += static_cast<float>(correct_found) / K;
    }

    float avg_accuracy = total_accuracy / NUM_TESTS;
    std::cout << "[INFO] Test 10 (Secure Clustering): Precision promedio (" << K << "-NN Accuracy) = " << avg_accuracy * 100.0f << "%\n";

    if (avg_accuracy < 0.7) {
        std::cerr << "[ERROR] Test 10 (Secure Clustering): La precision es demasiado baja.\n";
        return false;
    }
    std::cout << "[OK] Test 10 (Secure Clustering) pasó con una precision aceptable.\n";
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

    int cant = 5000; // Número de vectores SIFT a cargar
    
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

    // Añadir la llamada al nuevo test al final del main
    if (!testSecureLinearScan<SIFTVector>(sift_vectors)) overallOK = false;

    if (!testSecureClusteringKnn<SIFTVector>(sift_vectors)) overallOK = false;

    std::cout << "\n----------------------------------------\n";
    if (overallOK) {
        std::cout << "¡Felicidades! Todos los tests relevantes pasaron correctamente usando datos SIFT reales.\n";
        return 0;
    } else {
        std::cout << "Rayos. Algun test fallo. ¡A depurar se ha dicho!\n";
        return 1;
    }
}
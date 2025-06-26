#include <iostream>
#include <filesystem>
#include "FvecsReader.hpp"

int main() {
    // Rutas de los archivos SIFT
    std::string base_path = "siftsmall_base.fvecs";
    std::string query_path = "siftsmall_query.fvecs";
    std::string learn_path = "siftsmall_learn.fvecs";
    
    std::cout << "=== LECTOR DE DATASET SIFT ===" << std::endl;
    
    // Leer archivo base
    std::cout << "\n1. Leyendo archivo base..." << std::endl;
    if (std::filesystem::exists(base_path)) {
        auto size = std::filesystem::file_size(base_path);
        std::cout << "Archivo: " << base_path << " (tamaño: " << size << " bytes)" << std::endl;
        
        std::vector<SIFTNode> base_data = FvecsReader::read(base_path);
        if (!base_data.empty()) {
            std::cout << "✓ Leídos " << base_data.size() << " vectores base" << std::endl;
            std::cout << "  Dimensión: " << base_data[0].descriptor.size() << std::endl;
            std::cout << "  Primer vector: ";
            for (int i = 0; i < 10 && i < static_cast<int>(base_data[0].descriptor.size()); ++i) {
                std::cout << base_data[0].descriptor[i] << " ";
            }
            std::cout << "..." << std::endl;
        }
    } else {
        std::cout << "✗ No se encontró: " << base_path << std::endl;
    }
    
    // Leer archivo query
    std::cout << "\n2. Leyendo archivo query..." << std::endl;
    if (std::filesystem::exists(query_path)) {
        auto size = std::filesystem::file_size(query_path);
        std::cout << "Archivo: " << query_path << " (tamaño: " << size << " bytes)" << std::endl;
        
        std::vector<SIFTNode> query_data = FvecsReader::read(query_path);
        if (!query_data.empty()) {
            std::cout << "✓ Leídos " << query_data.size() << " vectores query" << std::endl;
            std::cout << "  Dimensión: " << query_data[0].descriptor.size() << std::endl;
        }
    } else {
        std::cout << "✗ No se encontró: " << query_path << std::endl;
    }
    
    // Leer archivo learn
    std::cout << "\n3. Leyendo archivo learn..." << std::endl;
    if (std::filesystem::exists(learn_path)) {
        auto size = std::filesystem::file_size(learn_path);
        std::cout << "Archivo: " << learn_path << " (tamaño: " << size << " bytes)" << std::endl;
        
        std::vector<SIFTNode> learn_data = FvecsReader::read(learn_path);
        if (!learn_data.empty()) {
            std::cout << "✓ Leídos " << learn_data.size() << " vectores learn" << std::endl;
            std::cout << "  Dimensión: " << learn_data[0].descriptor.size() << std::endl;
        }
    } else {
        std::cout << "✗ No se encontró: " << learn_path << std::endl;
    }
    
    // Verificar si existe el archivo groundtruth (formato .ivecs)
    std::string groundtruth_path = "siftsmall_groundtruth.ivecs";
    std::cout << "\n4. Verificando groundtruth..." << std::endl;
    if (std::filesystem::exists(groundtruth_path)) {
        auto size = std::filesystem::file_size(groundtruth_path);
        std::cout << "✓ Encontrado: " << groundtruth_path << " (tamaño: " << size << " bytes)" << std::endl;
        std::cout << "  Nota: Este archivo contiene enteros (formato .ivecs)" << std::endl;
    } else {
        std::cout << "✗ No se encontró: " << groundtruth_path << std::endl;
    }
    
    std::cout << "\n=== RESUMEN ===" << std::endl;
    std::cout << "Dataset SIFT cargado exitosamente!" << std::endl;
    std::cout << "Puedes usar estos datos para implementar algoritmos de búsqueda de vectores similares." << std::endl;
    
    return 0;
}
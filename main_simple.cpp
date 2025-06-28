#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <cmath>
#include "SIFTReader.hpp"

int main() {
    std::cout << "=== LECTOR SIMPLE DE DATASETS SIFT ===" << std::endl;
    
    // Cargar datos base
    std::cout << "\n1. Cargando datos base..." << std::endl;
    std::vector<SIFTVector> base_vectors = SIFTReader::readFile("siftsmall_base.fvecs");
    
    if (!base_vectors.empty()) {
        std::cout << "✓ Datos base cargados exitosamente!" << std::endl;
        SIFTReader::printInfo(base_vectors);
    } else {
        std::cout << "✗ Error al cargar datos base" << std::endl;
    }
    
    // Cargar datos de consulta
    std::cout << "\n2. Cargando datos de consulta..." << std::endl;
    std::vector<SIFTVector> query_vectors = SIFTReader::readFile("siftsmall_query.fvecs");
    
    if (!query_vectors.empty()) {
        std::cout << "✓ Datos de consulta cargados exitosamente!" << std::endl;
        SIFTReader::printInfo(query_vectors);
    } else {
        std::cout << "✗ Error al cargar datos de consulta" << std::endl;
    }
    
    // Demostrar uso de los objetos
    if (!base_vectors.empty() && !query_vectors.empty()) {
        std::cout << "\n3. Demostrando uso de los objetos..." << std::endl;
        
        // Calcular distancia entre primer vector de consulta y primeros 5 vectores base
        const SIFTVector& query = query_vectors[0];
        std::cout << "Distancias desde el primer vector de consulta:" << std::endl;
        
        for (int i = 0; i < 5 && i < static_cast<int>(base_vectors.size()); ++i) {
            float distance = query.distanceTo(base_vectors[i]);
            std::cout << "  Vector base " << i << " (ID: " << base_vectors[i].id 
                      << "): distancia = " << distance << std::endl;
        }
        
        // Mostrar cómo acceder a datos individuales
        std::cout << "\n4. Acceso a datos individuales:" << std::endl;
        std::cout << "Primer vector base:" << std::endl;
        std::cout << "  ID: " << base_vectors[0].id << std::endl;
        std::cout << "  Dimensión: " << base_vectors[0].getDimension() << std::endl;
        std::cout << "  Primeros 5 valores: ";
        for (size_t i = 0; i < 5; ++i) {
            std::cout << base_vectors[0][i] << " ";
        }
        std::cout << std::endl;
    }
    
    std::cout << "\n=== RESUMEN ===" << std::endl;
    std::cout << "✓ Vectores base cargados: " << base_vectors.size() << std::endl;
    std::cout << "✓ Vectores consulta cargados: " << query_vectors.size() << std::endl;
    std::cout << "¡Los datos están listos para usar en tus algoritmos!" << std::endl;
    
    return 0;
}
#ifndef SIFT_READER_HPP
#define SIFT_READER_HPP

#include <vector>
#include <string>

// Clase simple para encapsular cada vector SIFT
class SIFTVector {
public:
    std::vector<float> data;
    int id;
    
    // Constructor
    SIFTVector(const std::vector<float>& vector_data, int vector_id = 0) 
        : data(vector_data), id(vector_id) {}
    
    // Método para obtener la dimensión
    size_t getDimension() const {
        return data.size();
    }
    
    // Método para obtener un elemento específico
    float operator[](size_t index) const {
        return data[index];
    }
    
    // Método para calcular distancia euclidiana con otro vector
    float distanceTo(const SIFTVector& other) const {
        if (data.size() != other.data.size()) {
            throw std::runtime_error("Vectores de diferentes dimensiones");
        }
        
        float sum = 0.0f;
        for (size_t i = 0; i < data.size(); ++i) {
            float diff = data[i] - other.data[i];
            sum += diff * diff;
        }
        return sqrt(sum);
    }
};

// Clase para leer archivos SIFT
class SIFTReader {
public:
    // Método principal para leer archivos .fvecs y crear vector de objetos
    static std::vector<SIFTVector> readFile(const std::string& filepath) {
        std::vector<SIFTVector> vectors;
        std::ifstream file(filepath, std::ios::binary);
        
        if (!file.is_open()) {
            std::cerr << "Error: No se puede abrir el archivo " << filepath << std::endl;
            return vectors;
        }
        
        int vector_id = 0;
        
        while (!file.eof()) {
            // Leer dimensión
            int dimension;
            file.read(reinterpret_cast<char*>(&dimension), sizeof(int));
            
            if (file.eof()) break;
            
            // Leer vector de datos
            std::vector<float> vector_data(dimension);
            file.read(reinterpret_cast<char*>(vector_data.data()), dimension * sizeof(float));
            
            if (file.gcount() != dimension * sizeof(float)) break;
            
            // Crear objeto SIFTVector y agregarlo al vector
            vectors.emplace_back(vector_data, vector_id++);
        }
        
        file.close();
        return vectors;
    }
    
    // Método para mostrar información de los vectores cargados
    static void printInfo(const std::vector<SIFTVector>& vectors) {
        if (vectors.empty()) {
            std::cout << "No hay vectores cargados." << std::endl;
            return;
        }
        
        std::cout << "Total de vectores: " << vectors.size() << std::endl;
        std::cout << "Dimensión: " << vectors[0].getDimension() << std::endl;
        std::cout << "Primer vector (primeras 10 dimensiones): ";
        
        for (size_t i = 0; i < std::min(10UL, vectors[0].getDimension()); ++i) {
            std::cout << vectors[0][i] << " ";
        }
        std::cout << std::endl;
    }
};

#endif
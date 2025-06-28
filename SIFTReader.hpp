#ifndef SIFT_READER_HPP
#define SIFT_READER_HPP

#include <vector>
#include <string>
#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <random>
#include <algorithm>

constexpr float EPSILON = 1e-8f;

// Clase SIFTVector adaptada con los métodos de la clase Point
class SIFTVector {
public:
    std::vector<float> data;
    int id;
    
    // Constructores
    SIFTVector() : id(0) {}
    
    SIFTVector(const std::vector<float>& vector_data, int vector_id = 0) 
        : data(vector_data), id(vector_id) {}
    
    // Operadores aritméticos - suma
    SIFTVector operator+(const SIFTVector& other) const {
        if (data.size() != other.data.size()) {
            throw std::invalid_argument("Vectores de diferentes dimensiones");
        }
        
        std::vector<float> result_data(data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] + other.data[i];
        }
        return SIFTVector(result_data, id);
    }
    
    SIFTVector& operator+=(const SIFTVector& other) {
        if (data.size() != other.data.size()) {
            throw std::invalid_argument("Vectores de diferentes dimensiones");
        }
        
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] += other.data[i];
        }
        return *this;
    }
    
    // Operadores aritméticos - resta
    SIFTVector operator-(const SIFTVector& other) const {
        if (data.size() != other.data.size()) {
            throw std::invalid_argument("Vectores de diferentes dimensiones");
        }
        
        std::vector<float> result_data(data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] - other.data[i];
        }
        return SIFTVector(result_data, id);
    }
    
    SIFTVector& operator-=(const SIFTVector& other) {
        if (data.size() != other.data.size()) {
            throw std::invalid_argument("Vectores de diferentes dimensiones");
        }
        
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] -= other.data[i];
        }
        return *this;
    }
    
    // Operadores aritméticos - multiplicación por escalar
    SIFTVector operator*(float scalar) const {
        std::vector<float> result_data(data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] * scalar;
        }
        return SIFTVector(result_data, id);
    }
    
    SIFTVector& operator*=(float scalar) {
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] *= scalar;
        }
        return *this;
    }
    
    // Operadores aritméticos - división por escalar
    SIFTVector operator/(float scalar) const {
        if (std::abs(scalar) < EPSILON) {
            throw std::invalid_argument("Division by zero");
        }
        
        std::vector<float> result_data(data.size());
        for (std::size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] / scalar;
        }
        return SIFTVector(result_data, id);
    }
    
    SIFTVector& operator/=(float scalar) {
        if (std::abs(scalar) < EPSILON) {
            throw std::invalid_argument("Division by zero");
        }
        
        for (std::size_t i = 0; i < data.size(); ++i) {
            data[i] /= scalar;
        }
        return *this;
    }
    
    // Método para calcular la norma euclidiana
    float norm() const {
        float sum = 0.0f;
        for (std::size_t i = 0; i < data.size(); ++i) {
            sum += data[i] * data[i];
        }
        return std::sqrt(sum);
    }
    
    // Operadores de acceso por índice
    float operator[](std::size_t index) const {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }
    
    float& operator[](std::size_t index) {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }
    
    // Métodos estáticos
    static SIFTVector random(std::size_t dimension, float min = 0.0f, float max = 1.0f) {
        static std::random_device rd;
        static std::mt19937 eng(rd());
        std::uniform_real_distribution<float> dis(min, max);
        
        std::vector<float> coords(dimension);
        for (auto& c : coords) {
            c = dis(eng);
        }
        return SIFTVector(coords);
    }
    
    static float distance(const SIFTVector& v1, const SIFTVector& v2) {
        if (v1.data.size() != v2.data.size()) {
            throw std::invalid_argument("Vectores de diferentes dimensiones");
        }
        
        float sum = 0.0f;
        for (std::size_t i = 0; i < v1.data.size(); ++i) {
            float diff = v1.data[i] - v2.data[i];
            sum += diff * diff;
        }
        return std::sqrt(sum);
    }
    
    // Métodos originales mantenidos
    size_t getDimension() const {
        return data.size();
    }
    
    // Método distanceTo mantenido para compatibilidad
    float distanceTo(const SIFTVector& other) const {
        return distance(*this, other);
    }
};

// Clase SIFTReader mantenida igual
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
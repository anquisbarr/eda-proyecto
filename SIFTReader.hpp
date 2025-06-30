#ifndef SIFT_READER_HPP
#define SIFT_READER_HPP

#include <iostream>
#include <fstream>
#include <cmath>
#include <stdexcept>
#include <vector>
#include <string>
#include <algorithm>

constexpr std::size_t VDIM = 128; // Dimensión de los vectores SIFT

// Clase simple para encapsular cada vector SIFT
class SIFTVector {
    std::vector<float> data;
    
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
        return std::sqrt(sum);
    }
    
public:
    int id;

    // Constructor
    SIFTVector(const std::vector<float>& vector_data, int vector_id = 0) 
        : data(vector_data), id(vector_id) {}
    
    // Constructor por defecto
    SIFTVector() : data(VDIM), id(0) {}

    SIFTVector operator+ (const SIFTVector& other) const;
    SIFTVector& operator+=(const SIFTVector& other);
    SIFTVector operator- (const SIFTVector& other) const;
    SIFTVector& operator-=(const SIFTVector& other);
    SIFTVector operator* (float scalar) const;
    SIFTVector& operator*=(float scalar);
    SIFTVector operator/ (float scalar) const;
    SIFTVector& operator/=(float scalar);
    float norm() const;

    float  operator[](std::size_t index) const; 
    float& operator[](std::size_t index);

    static size_t getDimension();

    static float distance(const SIFTVector& p1, const SIFTVector& p2);
};
    
    // Método para obtener la dimensión
    size_t SIFTVector::getDimension() {
        return VDIM;
    }
    
    // Método para obtener un elemento específico
    float SIFTVector::operator[](size_t index) const {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }
    
    // Método para obtener un elemento específico (no const)
    float& SIFTVector::operator[](size_t index) {
        if (index >= data.size()) {
            throw std::out_of_range("Index out of range");
        }
        return data[index];
    }
    
    // Operadores aritméticos
    SIFTVector SIFTVector::operator+(const SIFTVector& other) const {
        if (data.size() != other.data.size()) {
            throw std::runtime_error("Vectores de diferentes dimensiones");
        }
        
        std::vector<float> result_data(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] + other.data[i];
        }
        return SIFTVector(result_data, id);
    }

    SIFTVector& SIFTVector::operator+=(const SIFTVector& other) {
        if (data.size() != other.data.size()) {
            throw std::runtime_error("Vectores de diferentes dimensiones");
        }
        
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] += other.data[i];
        }
        return *this;
    }

    SIFTVector SIFTVector::operator-(const SIFTVector& other) const {
        if (data.size() != other.data.size()) {
            throw std::runtime_error("Vectores de diferentes dimensiones");
        }
        
        std::vector<float> result_data(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] - other.data[i];
        }
        return SIFTVector(result_data, id);
    }

    SIFTVector& SIFTVector::operator-=(const SIFTVector& other) {
        if (data.size() != other.data.size()) {
            throw std::runtime_error("Vectores de diferentes dimensiones");
        }
        
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] -= other.data[i];
        }
        return *this;
    }

    SIFTVector SIFTVector::operator*(float scalar) const {
        std::vector<float> result_data(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] * scalar;
        }
        return SIFTVector(result_data, id);
    }

    SIFTVector& SIFTVector::operator*=(float scalar) {
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] *= scalar;
        }
        return *this;
    }

    SIFTVector SIFTVector::operator/(float scalar) const {
        if (std::abs(scalar) < 1e-8f) {
            throw std::invalid_argument("Division by zero");
        }
        
        std::vector<float> result_data(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            result_data[i] = data[i] / scalar;
        }
        return SIFTVector(result_data, id);
    }

    SIFTVector& SIFTVector::operator/=(float scalar) {
        if (std::abs(scalar) < 1e-8f) {
            throw std::invalid_argument("Division by zero");
        }
        
        for (size_t i = 0; i < data.size(); ++i) {
            data[i] /= scalar;
        }
        return *this;
    }
    
    // Método para calcular la norma euclidiana
    float SIFTVector::norm() const {
        float sum = 0.0f;
        for (size_t i = 0; i < data.size(); ++i) {
            sum += data[i] * data[i];
        }
        return std::sqrt(sum);
    }

    
    // Método estático para calcular distancia (compatible con templates)
    float SIFTVector::distance(const SIFTVector& v1, const SIFTVector& v2) {
        return v1.distanceTo(v2);
    }

    
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
            /*
            // Leer dimensión
            int dimension;
            file.read(reinterpret_cast<char*>(&dimension), sizeof(int));
            */
            
            if (file.eof()) break;
            
            // Leer vector de datos
            std::vector<float> vector_data(VDIM);
            file.read(reinterpret_cast<char*>(vector_data.data()), VDIM * sizeof(float));
            
            if (file.gcount() != VDIM * sizeof(float)) break;
            
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
        
        for (size_t i = 0; i < std::min(static_cast<size_t>(10), vectors[0].getDimension()); ++i) {
            std::cout << vectors[0][i] << " ";
        }
        std::cout << std::endl;
    }
};

#endif
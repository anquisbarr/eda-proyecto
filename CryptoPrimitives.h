#ifndef CRYPTO_PRIMITIVES_H
#define CRYPTO_PRIMITIVES_H

#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <numeric>
#include <algorithm>
#include <random>

// --- SIMULACIÓN DE AHE (Additive Homomorphic Encryption) ---

// Simula un texto cifrado. En una implementación real, esto sería
// un objeto matemático complejo (p. ej., un par de polinomios).
// Aquí, simplemente contiene el valor en texto plano para simular las operaciones.
template<typename T>
class AHECiphertext {
public:
    T plaintext_value; // "Oculto" para el servidor

    AHECiphertext(T value = T()) : plaintext_value(value) {}

    // Operación homomórfica: Suma de dos textos cifrados
    AHECiphertext<T> operator+(const AHECiphertext<T>& other) const {
        return AHECiphertext<T>(this->plaintext_value + other.plaintext_value);
    }

    // Operación homomórfica: Suma de un cifrado y un plano (el servidor puede hacer esto)
    AHECiphertext<T> operator+(const T& scalar) const {
        return AHECiphertext<T>(this->plaintext_value + scalar);
    }
    
    // Operación homomórfica: Multiplicación por una constante (el servidor puede hacer esto)
    AHECiphertext<T> operator*(const T& scalar) const {
        return AHECiphertext<T>(this->plaintext_value * scalar);
    }
};

// Simula el rol del Cliente, que tiene la clave secreta.
class AHEClient {
public:
    // En la vida real, aquí se generaría un par de claves (pública, secreta).
    // Nosotros no necesitamos simularlo.

    template<typename T>
    AHECiphertext<T> encrypt(const T& value) {
        // Simplemente envolvemos el valor en nuestra clase de texto cifrado.
        return AHECiphertext<T>(value);
    }

    template<typename T>
    T decrypt(const AHECiphertext<T>& ciphertext) {
        // Simplemente desenvolvemos el valor.
        return ciphertext.plaintext_value;
    }
};


// --- SIMULACIÓN DE GARBLED CIRCUITS (GC) ---

// Esta clase simula la interacción para ejecutar un circuito Top-K.
class GarbledCircuit {
public:
    // En la vida real, esto sería un protocolo complejo de varias rondas.
    // Lo simulamos como una única función estática que toma las "entradas" de
    // ambas partes y devuelve el resultado.
    // El cliente provee su parte de los datos, el servidor la suya.
    // El circuito solo revela el resultado final.
    template<typename PointType>
    static std::vector<PointType> secureTopK(
        const std::vector<long long>& client_shares, // Parte del cliente
        const std::vector<long long>& server_shares, // Parte del servidor
        const std::vector<PointType>& server_points, // Puntos originales del servidor
        size_t k
    ) {
        if (client_shares.size() != server_shares.size() || server_shares.size() != server_points.size()) {
            throw std::runtime_error("Inconsistencia de tamaños en Garbled Circuit.");
        }

        // 1. El circuito reconstruye internamente los valores completos.
        std::vector<long long> full_distances(client_shares.size());
        for (size_t i = 0; i < full_distances.size(); ++i) {
            full_distances[i] = client_shares[i] + server_shares[i];
        }

        // 2. Ejecuta la lógica de Top-K (ordenamiento) sobre los valores reconstruidos.
        std::vector<std::pair<long long, size_t>> dist_idx_pairs(full_distances.size());
        for(size_t i = 0; i < full_distances.size(); ++i) {
            dist_idx_pairs[i] = {full_distances[i], i};
        }
    
        k = std::min(k, dist_idx_pairs.size());
        std::partial_sort(dist_idx_pairs.begin(), dist_idx_pairs.begin() + k, dist_idx_pairs.end());

        // 3. Devuelve el resultado final (los k mejores puntos).
        std::vector<PointType> result;
        result.reserve(k);
        for(size_t i = 0; i < k; ++i) {
            result.push_back(server_points[dist_idx_pairs[i].second]);
        }
        
        return result;
    }

    // --- NUEVA FUNCIÓN: Simulación de la Funcionalidad F_aTOPk (Figura 4) ---
    template<typename PointType>
    static std::vector<PointType> secureApproximateTopK(
        const std::vector<long long>& client_shares,
        const std::vector<long long>& server_shares,
        const std::vector<PointType>& server_points,
        size_t k,
        size_t l_bins
    ) {
        if (client_shares.size() != server_shares.size() || server_shares.size() != server_points.size()) {
            throw std::runtime_error("Inconsistencia de tamaños en Garbled Circuit (Approx).");
        }

        std::vector<long long> full_distances(client_shares.size());
        for (size_t i = 0; i < full_distances.size(); ++i) {
            full_distances[i] = client_shares[i] + server_shares[i];
        }

        if (l_bins <= k || l_bins >= server_points.size()) {
            // El fallback a Top-K exacto es lo más seguro y simple.
            std::vector<std::pair<long long, size_t>> dist_idx_pairs(full_distances.size());
            for(size_t i = 0; i < full_distances.size(); ++i) dist_idx_pairs[i] = {full_distances[i], i};
            
            k = std::min(k, dist_idx_pairs.size());
            std::partial_sort(dist_idx_pairs.begin(), dist_idx_pairs.begin() + k, dist_idx_pairs.end());

            std::vector<PointType> result;
            result.reserve(k);
            for(size_t i = 0; i < k; ++i) result.push_back(server_points[dist_idx_pairs[i].second]);
            return result;
        }

        // --- Lógica de Aproximación (Shuffle and Bin) ---
        std::vector<std::pair<long long, size_t>> dist_idx_pairs(full_distances.size());
        for(size_t i = 0; i < full_distances.size(); ++i) {
            dist_idx_pairs[i] = {full_distances[i], i};
        }

        static std::mt19937 g(42);
        std::shuffle(dist_idx_pairs.begin(), dist_idx_pairs.end(), g);

        using Elem = std::pair<long long, PointType>;
        std::vector<Elem> candidates;
        candidates.reserve(l_bins);

        size_t bin_size = (dist_idx_pairs.size() + l_bins - 1) / l_bins;
        for (size_t i = 0; i < l_bins; ++i) {
            size_t start_idx = i * bin_size;
            size_t end_idx = std::min(start_idx + bin_size, dist_idx_pairs.size());
            if (start_idx >= end_idx) continue;
            
            auto min_it = std::min_element(dist_idx_pairs.begin() + start_idx, dist_idx_pairs.begin() + end_idx);
            
            // Guardamos tanto el punto como su distancia para el refinamiento.
            candidates.emplace_back(min_it->first, server_points[min_it->second]);
        }

        // 2b. Refinamiento: ordenar los 'l' candidatos por su distancia (ya la tenemos).
        k = std::min(k, candidates.size());
        auto cmp = [](const Elem& a, const Elem& b) { return a.first < b.first; };
        std::partial_sort(candidates.begin(), candidates.begin() + k, candidates.end(), cmp);

        // 3. Devolver el resultado final.
        std::vector<PointType> result;
        result.reserve(k);
        for(size_t i = 0; i < k; ++i) {
            result.push_back(candidates[i].second);
        }
        return result;
    }
};

#endif // CRYPTO_PRIMITIVES_H
#ifndef SECURE_SANNS_H
#define SECURE_SANNS_H

#include <vector>
#include <map>
#include <stdexcept>
#include "CryptoPrimitives.h" // Usamos nuestras primitivas simuladas
#include "../Sanns/SANNS.h"          // Necesitamos naiveTopKSquared, etc.

static constexpr float FLOAT_TOL = 1e-6f;

template<typename PointType>
bool equalPoint(const PointType& a, const PointType& b) { 
    return PointType::distance(a, b) < FLOAT_TOL; 
}

template<typename PointType>
double get_dist_sq(const PointType& p1, const PointType& p2) {
    double dist_sq = 0.0;
    for(size_t i = 0; i < PointType::getDimension(); ++i) {
        double diff = static_cast<double>(p1[i]) - static_cast<double>(p2[i]);
        dist_sq += diff * diff;
    }
    return dist_sq;
}

// Función sobrecargada para crear un punto "cero"
template<typename PointType>
PointType createZeroPoint() {
    PointType zero;
    // Inicializar todas las dimensiones a 0
    for(size_t i = 0; i < PointType::getDimension(); ++i) {
        zero[i] = 0.0f;
    }
    return zero;
}

// --- ROL DEL SERVIDOR ---
// Esta clase encapsula la lógica que el servidor ejecutaría.
template<typename PointType>
class SecureServer {
private:
    // El servidor ahora almacena la estructura de clustering pre-calculada.
    std::map<int, PointType> all_points_map; // Para búsqueda por ID
    SannsDB<PointType> sanns_db_structure;

public:
    const std::vector<PointType>& db_points; // El servidor tiene la BD en texto plano

    // Constructor simple - inicializa SannsDB con parámetros por defecto
    SecureServer(const std::vector<PointType>& db) 
        : db_points(db), sanns_db_structure() {}

    // El servidor recibe la BD y los hiperparámetros para construir la estructura.
    SecureServer(const std::vector<PointType>& db, size_t m, float alpha, size_t u, size_t l) 
        : db_points(db), sanns_db_structure(m, alpha, u, l) 
    {
        // Pre-procesamiento: El servidor construye la SannsDB.
        sanns_db_structure.build(db);
        for(const auto& p : db) {
            all_points_map[p.id] = p;
        }
    }

    // Método para obtener los datos de la estructura (no es parte del protocolo, es para nosotros)
    const SannsDB<PointType>& getSannsDB() const { return sanns_db_structure; }

    // El servidor recibe la query cifrada y calcula las distancias al cuadrado homomórficamente.
    // Devuelve su "parte" del resultado para el Garbled Circuit.
    std::vector<long long> computeDistancesAndShare(
        const std::vector<AHECiphertext<double>>& encrypted_query,
        const std::vector<AHECiphertext<double>>& encrypted_query_sq_norm
    ) {
        std::vector<long long> server_distance_shares;
        server_distance_shares.reserve(db_points.size());

        constexpr double SCALING_FACTOR_D = 1e12;

        // La distancia al cuadrado es ||q-p||^2 = ||q||^2 - 2*<q,p> + ||p||^2
        // El servidor conoce ||p||^2 y puede calcular <q,p> homomórficamente.
        
        for (const auto& p : db_points) {
            // Calcular el producto escalar <q,p> homomórficamente
            AHECiphertext<double> encrypted_dot_product; // Inicia en 0
            for (size_t i = 0; i < PointType::getDimension(); ++i) {
                // encrypted_query[i] * p[i]
                encrypted_dot_product = encrypted_dot_product + (encrypted_query[i] * static_cast<double>(p[i]));
            }

            // El servidor calcula su parte: -2*<q,p> + ||p||^2
            // El cliente añadirá ||q||^2
            double p_sq_norm = get_dist_sq(p, PointType()); // ||p||^2
            
            // Simulación de "secret sharing": el servidor genera su parte.
            // Para simplificar, aquí el servidor calcula una parte de la distancia
            // y el cliente calculará la otra.
            AHECiphertext<double> server_part_encrypted = (encrypted_dot_product * -2.0) + p_sq_norm;
            
            // En un protocolo real, el servidor no puede desencriptar.
            // Aquí simulamos que el servidor tiene una "participación" del resultado.
            // Para nuestro mock, el servidor simplemente calcula su parte en texto plano.
            // El servidor no conoce <q,p>, solo -2*Enc(<q,p>). Pero para la simulación,
            // vamos a pretender que el resultado se divide mágicamente.
            
            // Para simplificar la simulación, vamos a hacer que el servidor calcule
            // la distancia completa usando el cifrado y luego el cliente la "descifre" y la divida.
            // En un protocolo real, la división se haría con GC.
            
            AHECiphertext<double> encrypted_dist_sq = encrypted_query_sq_norm[0] + server_part_encrypted;
            
            // --- Simplificación para la simulación ---
            // El servidor no puede hacer esto. Pero para que el flujo funcione,
            // asumimos que el resultado final se reparte.
            long long full_dist = static_cast<long long>(encrypted_dist_sq.plaintext_value * SCALING_FACTOR_D);
            long long server_share = full_dist / 2; // El servidor se queda con la mitad
            server_distance_shares.push_back(server_share);
        }
        return server_distance_shares;
    }

    // --- NUEVOS MÉTODOS PARA CLUSTERING ---

    // El servidor calcula homomórficamente las distancias a un conjunto de puntos (centroides o stash).
    std::vector<long long> computeDistancesToPoints(
        const std::vector<PointType>& points,
        const std::vector<AHECiphertext<double>>& encrypted_query,
        const AHECiphertext<double>& encrypted_query_sq_norm
    ) {
        std::vector<long long> server_distance_shares;
        server_distance_shares.reserve(points.size());
        constexpr double SCALING_FACTOR_D = 1e12;

        for (const auto& p : points) {
            AHECiphertext<double> encrypted_dot_product;
            for (size_t i = 0; i < PointType::getDimension(); ++i) {
                encrypted_dot_product = encrypted_dot_product + (encrypted_query[i] * static_cast<double>(p[i]));
            }
            double p_sq_norm = get_dist_sq(p, createZeroPoint<PointType>());
            AHECiphertext<double> server_part_encrypted = (encrypted_dot_product * -2.0) + p_sq_norm;
            AHECiphertext<double> encrypted_dist_sq = encrypted_query_sq_norm + server_part_encrypted;

            long long full_dist = static_cast<long long>(encrypted_dist_sq.plaintext_value * SCALING_FACTOR_D);
            long long server_share = full_dist / 2;
            server_distance_shares.push_back(server_share);
        }
        return server_distance_shares;
    }
};


// --- PROTOCOLO SECURE LINEAR SCAN ---
// Esta función orquesta la interacción entre el cliente y el servidor.
template<typename PointType>
std::vector<PointType> secureLinearScanKnn(
    const PointType& query,
    const std::vector<PointType>& allPoints, // Base de datos completa
    size_t k
) {
    // --- 1. Setup ---
    AHEClient client;
    SecureServer<PointType> server(allPoints);

    // --- 2. Fase de Cliente (Preparación) ---
    // El cliente cifra su query y su norma al cuadrado.
    std::vector<AHECiphertext<double>> encrypted_query;
    encrypted_query.reserve(PointType::getDimension());
    for (size_t i = 0; i < PointType::getDimension(); ++i) {
        encrypted_query.push_back(client.encrypt(static_cast<double>(query[i])));
    }
    
    double query_sq_norm = get_dist_sq(query, PointType());
    std::vector<AHECiphertext<double>> encrypted_query_sq_norm;
    encrypted_query_sq_norm.push_back(client.encrypt(query_sq_norm));

    // --- 3. Interacción: Cliente -> Servidor -> Cliente ---
    // El cliente envía la query cifrada al servidor.
    // El servidor calcula las distancias y devuelve su "share".
    std::vector<long long> server_distance_shares = server.computeDistancesAndShare(encrypted_query, encrypted_query_sq_norm);
    
    // --- 4. Fase de Cliente (Cálculo de su share) ---
    // En nuestra simulación simplificada, el cliente calcula la distancia completa
    // y luego su propia parte para el Garbled Circuit.
    std::vector<long long> client_distance_shares;
    client_distance_shares.reserve(allPoints.size());
    constexpr double SCALING_FACTOR_D = 1e12;
    for(size_t i = 0; i < allPoints.size(); ++i) {
        double dist_sq = get_dist_sq(query, allPoints[i]);
        long long full_dist = static_cast<long long>(dist_sq * SCALING_FACTOR_D);
        long long client_share = full_dist - server_distance_shares[i]; // El cliente calcula el resto
        client_distance_shares.push_back(client_share);
    }
    
    // --- 5. Interacción: Garbled Circuit para Top-K ---
    // Cliente y Servidor introducen sus "shares" en el circuito.
    // El circuito devuelve el resultado final al cliente.
    std::vector<PointType> final_result = GarbledCircuit::secureTopK<PointType>(
        client_distance_shares,
        server_distance_shares,
        allPoints, // El circuito necesita los puntos originales para devolverlos
        k
    );

    return final_result;
}

// --- PROTOCOLO SECURE CLUSTERING SCAN ---
template<typename PointType>
std::vector<PointType> secureClusteringKnn(
    const PointType& query,
    SecureServer<PointType>& server, // Pasamos el servidor pre-calculado
    size_t k
) {
    // --- 1. Setup ---
    AHEClient client;
    const SannsDB<PointType>& db_structure = server.getSannsDB();
    const auto& params = db_structure.getParams();

    // --- 2. Fase Cliente: Preparar Query Cifrada ---
    std::vector<AHECiphertext<double>> encrypted_query;
    for (size_t i = 0; i < PointType::getDimension(); ++i) {
        encrypted_query.push_back(client.encrypt(static_cast<double>(query[i])));
    }
    AHECiphertext<double> encrypted_query_sq_norm = client.encrypt(get_dist_sq(query, PointType()));
    
    std::vector<PointType> final_candidates;
    constexpr double SCALING_FACTOR_D = 1e12;

    // --- 3. Búsqueda en los Grupos de Clústeres ---
    for (const auto& group : db_structure.getGroups()) {
        std::vector<PointType> centers;
        for(const auto& cluster : group.clusters) centers.push_back(cluster.center);

        // a. AHE para distancias a centroides
        auto server_center_shares = server.computeDistancesToPoints(centers, encrypted_query, encrypted_query_sq_norm);
        
        // b. Cliente calcula su share
        std::vector<long long> client_center_shares;
        for(size_t i=0; i<centers.size(); ++i) {
            long long full_dist = static_cast<long long>(get_dist_sq(query, centers[i]) * SCALING_FACTOR_D);
            client_center_shares.push_back(full_dist - server_center_shares[i]);
        }

        // c. GC para seleccionar los mejores centroides
        std::vector<PointType> best_centers = GarbledCircuit::secureApproximateTopK(
            client_center_shares, server_center_shares, centers, params.clusters_to_retrieve_u, params.approx_topk_bins_l
        );

        // d. Recuperar puntos (SIMPLIFICACIÓN SIN DORAM)
        for(const auto& center : best_centers) {
            for(const auto& cluster : group.clusters) {
                if(equalPoint(center, cluster.center)) {
                    final_candidates.insert(final_candidates.end(), cluster.points.begin(), cluster.points.end());
                    break;
                }
            }
        }
    }
    
    // --- 4. Búsqueda en el Stash ---
    const auto& stash = db_structure.getStash();
    if (!stash.empty()) {
        // a. AHE para distancias al stash
        auto server_stash_shares = server.computeDistancesToPoints(stash, encrypted_query, encrypted_query_sq_norm);
        
        // b. Cliente calcula su share
        std::vector<long long> client_stash_shares;
        for(size_t i=0; i<stash.size(); ++i) {
            long long full_dist = static_cast<long long>(get_dist_sq(query, stash[i]) * SCALING_FACTOR_D);
            client_stash_shares.push_back(full_dist - server_stash_shares[i]);
        }

        // c. GC para seleccionar los mejores del stash
        std::vector<PointType> stash_neighbors = GarbledCircuit::secureApproximateTopK(
            client_stash_shares, server_stash_shares, stash, k, params.approx_topk_bins_l
        );
        final_candidates.insert(final_candidates.end(), stash_neighbors.begin(), stash_neighbors.end());
    }

    // --- 5. Combinación Final ---
    // En un protocolo real, esto sería otro ciclo AHE+GC.
    // Para simularlo, simplemente hacemos el Top-K final en texto plano en el lado del cliente.
    return naiveTopKSquared(query, final_candidates, k);
}

#endif // SECURE_SANNS_H
#ifndef SSPTREE_H
#define SSPTREE_H

#include <vector>
#include <limits>
#include <algorithm>
#include <string>
#include <cmath>
#include <stdexcept>
#include <queue>
#include <utility>
#include "Point.h"
#include "Sphere.h"

#include "SANNS.h" // Para usar el k-means de SANNS

//constexpr float EPSILON = 1e-8f;

template<typename PointType>
class SSPTree;


template<typename PointType>
class SSPNode {
private:
    Sphere<PointType> _boundingSphere;
    SSPNode<PointType>* _parent;
    std::vector<PointType>    _points;
    std::vector<SSPNode<PointType>*> _children;
    bool _isLeaf;

    friend class SSPTree<PointType>; // Permitir que SSPTree acceda a miembros privados

public:
    SSPNode(bool isLeaf = false, SSPNode<PointType>* parent = nullptr) 
        : _parent(parent), _isLeaf(isLeaf) {}
    
    ~SSPNode() {
        for (auto child : _children) {
            delete child;
        }
    }

    // --- Getters ---
    bool                  getIsLeaf() const { return _isLeaf; }
    SSPNode<PointType>* getParent() const { return _parent; }
    const Sphere<PointType>&         getBoundingSphere() const { return _boundingSphere; }
    const std::vector<PointType>&   getPoints() const { return   _points; }
    const std::vector<SSPNode<PointType>*>& getChildren() const { return _children; }
    std::size_t           getNumPoints() const { return   _points.size(); }
    std::size_t           getNumChildren() const { return _children.size(); }

    // --- Setters (usados por SSPTree) ---
    void setBoundingSphere(const Sphere<PointType>& sphere) { _boundingSphere = sphere; }
    void setParent        (SSPNode<PointType>* parent)      { _parent = parent; }
    void setIsLeaf        (bool isLeaf)          { _isLeaf = isLeaf; }

    void recalculateBoundingSphere() {
        if (_isLeaf) {
            if (_points.empty()) {
                _boundingSphere = Sphere<PointType>();
                return;
            }
            // Aproximación: centroide y radio máximo
            PointType centroid;
            for(const auto& p : _points) centroid += p;
            centroid /= static_cast<float>(_points.size());

            float maxDist = 0.0f;
            for(const auto& p : _points) {
                maxDist = std::max(maxDist, PointType::distance(centroid, p));
            }
            _boundingSphere = Sphere<PointType>(centroid, maxDist);

        } else { // Nodo interno
            if (_children.empty()) {
                _boundingSphere = Sphere<PointType>();
                return;
            }
            // Empezamos con la esfera del primer hijo y la expandimos
            _boundingSphere = _children[0]->getBoundingSphere();
            for (size_t i = 1; i < _children.size(); ++i) {
                _boundingSphere.expandToInclude(_children[i]->getBoundingSphere());
            }
        }
    }
};


// Declaración adelantada de la función que usaremos.
// Esto ayuda a mantener los headers desacoplados.
template<typename PointType>
std::vector<PointType> naiveTopKSquared(const PointType& query, const std::vector<PointType>& points, size_t k);

template<typename PointType>
class SSPTree {
private:
    SSPNode<PointType>* _root;
    std::size_t _maxEntries;
    
    /**
     * @brief Implementación del algoritmo K-Means para dividir un conjunto de puntos en 2 clústeres.
     * 
     * Este método sigue el algoritmo heurístico estándar para minimizar la suma de las distancias cuadradas intra-clúster.
     * 1. Inicialización: Escoge dos puntos iniciales como centroides.
     * 2. Iteración:
     *    a. Paso de Asignación: Asigna cada punto al clúster de su centroide más cercano.
     *    b. Paso de Actualización: Recalcula cada centroide como la media de los puntos asignados a su clúster.
     * 3. Convergencia: El bucle se detiene tras un número fijo de iteraciones o si los centroides dejan de moverse.
     * 
     * @param points_to_cluster El vector de puntos que se va a dividir.
     * @return Un vector de enteros (0 o 1) que indica la asignación de clúster para cada punto de entrada.
     */
    std::vector<int> kmeansClustering(const std::vector<PointType>& points_to_cluster) {
        const int K = 2;
        const int MAX_ITER = 15;
        const float CONVERGENCE_TOL = 1e-6f;
        std::vector<int> assignments(points_to_cluster.size(), 0);

        if (points_to_cluster.size() <= K) {
             for(size_t i = 0; i < points_to_cluster.size(); ++i) assignments[i] = i % K;
             return assignments;
        }

        // --- 1. Inicialización de Centroides ---
        // Estrategia: Elegir los dos puntos más distantes entre sí para un mejor comienzo.
        PointType centroid1, centroid2;
        float max_dist_sq = -1.0f;
        for (size_t i = 0; i < points_to_cluster.size(); ++i) {
            for (size_t j = i + 1; j < points_to_cluster.size(); ++j) {
                float d_sq = (points_to_cluster[i] - points_to_cluster[j]).norm();
                d_sq *= d_sq; // Usamos distancia al cuadrado para evitar sqrt
                if (d_sq > max_dist_sq) {
                    max_dist_sq = d_sq;
                    centroid1 = points_to_cluster[i];
                    centroid2 = points_to_cluster[j];
                }
            }
        }

        // --- 2. Bucle de Iteración ---
        for (int iter = 0; iter < MAX_ITER; ++iter) {
            // --- 2a. Paso de Asignación ---
            for (size_t i = 0; i < points_to_cluster.size(); ++i) {
                // Se usa la distancia al cuadrado para ser coherente con el objetivo de K-Means y evitar sqrt
                float dist_sq1 = PointType::distance(points_to_cluster[i], centroid1);
                dist_sq1 *= dist_sq1;
                float dist_sq2 = PointType::distance(points_to_cluster[i], centroid2);
                dist_sq2 *= dist_sq2;
                assignments[i] = (dist_sq1 < dist_sq2) ? 0 : 1;
            }

            // --- 2b. Paso de Actualización ---
            PointType new_centroid1, new_centroid2;
            int count1 = 0, count2 = 0;
            for (size_t i = 0; i < points_to_cluster.size(); ++i) {
                if (assignments[i] == 0) {
                    new_centroid1 += points_to_cluster[i];
                    count1++;
                } else {
                    new_centroid2 += points_to_cluster[i];
                    count2++;
                }
            }
            
            // Si un clúster queda vacío, se mantiene el centroide anterior para evitar divisiones inválidas.
            if (count1 > 0) new_centroid1 /= static_cast<float>(count1); else new_centroid1 = centroid1;
            if (count2 > 0) new_centroid2 /= static_cast<float>(count2); else new_centroid2 = centroid2;

            // --- 3. Verificación de Convergencia ---
            if (PointType::distance(centroid1, new_centroid1) < CONVERGENCE_TOL &&
                PointType::distance(centroid2, new_centroid2) < CONVERGENCE_TOL) {
                break; // Los centroides han convergido.
            }

            centroid1 = new_centroid1;
            centroid2 = new_centroid2;
        }

        return assignments;
    }

    SSPNode<PointType>* chooseSubtree(SSPNode<PointType>* node, const PointType& point) const {
        if (node->getIsLeaf()) return node;
        
        SSPNode<PointType>* bestChild = nullptr;
        float minDistance = std::numeric_limits<float>::max();

        for (auto child : node->getChildren()) {
            float dist = PointType::distance(child->getBoundingSphere().center, point);
            if (dist < minDistance) {
                minDistance = dist;
                bestChild = child;
            }
        }
        return bestChild;
    }
    
    void splitNode(SSPNode<PointType>* node) {
        SSPNode<PointType>* sibling = new SSPNode<PointType>(node->getIsLeaf());
        std::vector<int> assignments;
        
        if (node->getIsLeaf()) { // --- División de un nodo hoja ---
            std::vector<PointType> allPoints = node->_points;
            assignments = kmeansClustering(allPoints);
            
            // Distribuir puntos en los nodos según el clustering
            node->_points.clear();
            for (size_t i = 0; i < allPoints.size(); ++i) {
                if (assignments[i] == 0) node->_points.push_back(allPoints[i]);
                else sibling->_points.push_back(allPoints[i]);
            }

        } else { // --- División de un nodo interno ---
            std::vector<SSPNode<PointType>*> allChildren = node->_children;
            std::vector<PointType> centroids;
            centroids.reserve(allChildren.size());
            for(auto child : allChildren) {
                centroids.push_back(child->getBoundingSphere().center);
            }
            assignments = kmeansClustering(centroids);
            
            // Distribuir hijos en los nodos según el clustering de sus centroides
            node->_children.clear();
            for (size_t i = 0; i < allChildren.size(); ++i) {
                if (assignments[i] == 0) {
                    node->_children.push_back(allChildren[i]);
                    allChildren[i]->setParent(node);
                } else {
                    sibling->_children.push_back(allChildren[i]);
                    allChildren[i]->setParent(sibling);
                }
            }
        }
        
        node->recalculateBoundingSphere();
        sibling->recalculateBoundingSphere();
        
        // --- Manejar el padre ---
        if (node == _root) {
            SSPNode<PointType>* newRoot = new SSPNode<PointType>(false);
            newRoot->_children.push_back(node);
            newRoot->_children.push_back(sibling);
            node->setParent(newRoot);
            sibling->setParent(newRoot);
            newRoot->recalculateBoundingSphere();
            _root = newRoot;
        } else {
            SSPNode<PointType>* parent = node->getParent();
            sibling->setParent(parent);
            parent->_children.push_back(sibling);
            
            if (parent->getNumChildren() > _maxEntries) {
                splitNode(parent);
            } else {
                SSPNode<PointType>* current = parent;
                while(current) {
                    current->recalculateBoundingSphere();
                    current = current->getParent();
                }
            }
        }
    }

    void adjustTree(SSPNode<PointType>* node){
        SSPNode<PointType>* current = node;
        while(current != nullptr){
            current->recalculateBoundingSphere();
            if( (current->getIsLeaf() && current->getNumPoints() > _maxEntries) ||
                (!current->getIsLeaf() && current->getNumChildren() > _maxEntries) ){
                splitNode(current);
                break;
            }
            current = current->getParent();
        }
    }

    // --- NUEVO HELPER PRIVADO ---
    // Desciende por el árbol para encontrar la hoja más prometedora para un punto.
    SSPNode<PointType>* findBestLeaf(const PointType& point) const {
        if (!_root) return nullptr;
        SSPNode<PointType>* node = _root;
        while (node && !node->getIsLeaf()) {
            node = chooseSubtree(node, point);
        }
        return node;
    }

    void collectPointsFromSubtree(const SSPNode<PointType>* node, std::vector<PointType>& outPoints) const {
        if (!node) return;
        
        if (node->getIsLeaf()) {
            const auto& points = node->getPoints();
            outPoints.insert(outPoints.end(), points.begin(), points.end());
        } else {
            for (auto child : node->getChildren()) {
                collectPointsFromSubtree(child, outPoints);
            }
        }
    }

public:
    SSPTree() : _maxEntries(15), _root(nullptr) {}
    explicit SSPTree(std::size_t maxEntries) : _maxEntries(maxEntries), _root(nullptr) {}
    ~SSPTree() { delete _root; }

    SSPNode<PointType>* getRoot() const { return _root; }

    void insert(const PointType& point) {
        if (!_root) {
            _root = new SSPNode<PointType>(true); // El primer nodo es una hoja
        }

        SSPNode<PointType>* leaf = _root;
        while (!leaf->getIsLeaf()) {
            leaf = chooseSubtree(leaf, point);
        }
        
        leaf->_points.push_back(point);
        adjustTree(leaf);
    }

    bool search(const PointType& point) const {
        if (!_root) return false;

        std::queue<SSPNode<PointType>*> q;
        q.push(_root);
        
        while (!q.empty()) {
            SSPNode<PointType>* current = q.front();
            q.pop();
            
            // Poda: si el punto no puede estar dentro de la esfera de contorno.
            if (PointType::distance(current->getBoundingSphere().center, point) > current->getBoundingSphere().radius + 1e-6f) {
                continue;
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    if (PointType::distance(p, point) < 1e-6f) {
                        return true;
                    }
                }
            } else {
                for (auto child : current->getChildren()) {
                    q.push(child);
                }
            }
        }
        return false;
    }

    std::vector<PointType> rangeQuery(const Sphere<PointType>& sphere) const {
        std::vector<PointType> result;
        if (!_root) return result;

        std::queue<SSPNode<PointType>*> q;
        q.push(_root);

        while (!q.empty()) {
            SSPNode<PointType>* current = q.front();
            q.pop();
            
            if (!current->getBoundingSphere().intersects(sphere)) {
                continue;
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    if (PointType::distance(sphere.center, p) <= sphere.radius + 1e-6f) {
                        result.push_back(p);
                    }
                }
            } else {
                for (auto child : current->getChildren()) {
                    q.push(child);
                }
            }
        }
        return result;
    }
    
    std::vector<PointType> kNearestNeighbors(const PointType& point, std::size_t k) const {
        std::vector<PointType> result;
        if (!_root || k == 0) return result;
        
        using ResultElem = std::pair<float, PointType>;
        auto cmp_Rel = [](const ResultElem& a, const ResultElem& b) { return a.first > b.first ? true : false; };
        std::priority_queue<ResultElem, std::vector<ResultElem>, decltype(cmp_Rel)> best_points(cmp_Rel);

        using QueueElem = std::pair<float, SSPNode<PointType>*>;
        auto cmp_Qel = [](const QueueElem& a, const QueueElem& b) { return a.first > b.first ? true : false; };
        std::priority_queue<QueueElem, std::vector<QueueElem>, decltype(cmp_Qel)> pq(cmp_Qel);

        pq.push({0.0f, _root});

        while (!pq.empty()) {
            float dist_to_node_boundary = pq.top().first;
            SSPNode<PointType>* current = pq.top().second;
            pq.pop();
            
            if (best_points.size() == k && dist_to_node_boundary > best_points.top().first) {
                break;
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    float dist = PointType::distance(point, p);
                    if (best_points.size() < k) {
                        best_points.push({dist, p});
                    } else if (dist < best_points.top().first) {
                        best_points.pop();
                        best_points.push({dist, p});
                    }
                }
            } else {
                for (auto child : current->getChildren()) {
                    float dist_to_center = PointType::distance(point, child->getBoundingSphere().center);
                    float dist_to_edge = std::max(0.0f, dist_to_center - child->getBoundingSphere().radius);
                    
                    if(best_points.size() < k || dist_to_edge < best_points.top().first){
                        pq.push({dist_to_edge, child});
                    }
                }
            }
        }
        
        result.reserve(best_points.size());
        while (!best_points.empty()) {
            result.push_back(best_points.top().second);
            best_points.pop();
        }
        std::reverse(result.begin(), result.end());
        return result;
    }

    // --- NUEVO MÉTODO EXPERIMENTAL ---
    // Utiliza Best-First Search con una cola de prioridad.
    std::vector<PointType> experimentalKnn(const PointType& query, size_t k, size_t search_budget = 256) const {
        if (!_root) return {};

        std::vector<PointType> candidate_points;

        // --- Cola de Prioridad (Min-Heap) ---
        // Almacena: {distancia_al_borde, puntero_al_nodo}
        using QueueElement = std::pair<float, SSPNode<PointType>*>;
        std::priority_queue<
            QueueElement,
            std::vector<QueueElement>,
            std::greater<QueueElement> // std::greater hace que sea un min-heap
        > nodes_to_visit;

        // 1. Añadir la raíz a la cola
        float root_dist = std::max(0.0f, PointType::distance(query, _root->getBoundingSphere().center) - _root->getBoundingSphere().radius);
        nodes_to_visit.push({root_dist, _root});

        size_t nodes_explored = 0;

        // 2. Bucle de búsqueda del mejor primero
        while (!nodes_to_visit.empty() && nodes_explored < search_budget) {
            // 2a. Obtener el nodo más prometedor
            SSPNode<PointType>* current = nodes_to_visit.top().second;
            nodes_to_visit.pop();
            nodes_explored++;

            // 2b. Procesar el nodo
            if (current->getIsLeaf()) {
                // Si es una hoja, recolectamos sus puntos
                const auto& points = current->getPoints();
                candidate_points.insert(candidate_points.end(), points.begin(), points.end());
            } else {
                // Si es un nodo interno, añadimos todos sus hijos a la cola
                for (auto child : current->getChildren()) {
                    // Calcular la distancia al borde de la esfera del hijo
                    float dist_to_center = PointType::distance(query, child->getBoundingSphere().center);
                    float dist_to_edge = std::max(0.0f, dist_to_center - child->getBoundingSphere().radius);
                    nodes_to_visit.push({dist_to_edge, child});
                }
            }
        }
        
        // 3. Refinar los candidatos recolectados
        // Se usa naiveTopK (con distancia real) por consistencia
        return naiveTopK(query, candidate_points, k);
    }
};

#endif // SSPTREE_H
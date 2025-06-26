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


class SSPTree;

class SSPNode {
private:
    Sphere _boundingSphere;
    SSPNode* _parent;
    std::vector<Point>    _points;
    std::vector<SSPNode*> _children;
    bool _isLeaf;

    friend class SSPTree; // Permitir que SSPTree acceda a miembros privados

public:
    SSPNode(bool isLeaf = false, SSPNode* parent = nullptr) 
        : _parent(parent), _isLeaf(isLeaf) {}
    
    ~SSPNode() {
        for (auto child : _children) {
            delete child;
        }
    }

    // --- Getters ---
    bool                  getIsLeaf() const { return _isLeaf; }
    SSPNode* getParent() const { return _parent; }
    const Sphere&         getBoundingSphere() const { return _boundingSphere; }
    const std::vector<Point>&   getPoints() const { return   _points; }
    const std::vector<SSPNode*>& getChildren() const { return _children; }
    std::size_t           getNumPoints() const { return   _points.size(); }
    std::size_t           getNumChildren() const { return _children.size(); }

    // --- Setters (usados por SSPTree) ---
    void setBoundingSphere(const Sphere& sphere) { _boundingSphere = sphere; }
    void setParent        (SSPNode* parent)      { _parent = parent; }
    void setIsLeaf        (bool isLeaf)          { _isLeaf = isLeaf; }

    void recalculateBoundingSphere() {
        if (_isLeaf) {
            if (_points.empty()) {
                _boundingSphere = Sphere();
                return;
            }
            // Aproximación: centroide y radio máximo
            Point centroid;
            for(const auto& p : _points) centroid += p;
            centroid /= static_cast<float>(_points.size());

            float maxDist = 0.0f;
            for(const auto& p : _points) {
                maxDist = std::max(maxDist, Point::distance(centroid, p));
            }
            _boundingSphere = Sphere(centroid, maxDist);

        } else { // Nodo interno
            if (_children.empty()) {
                _boundingSphere = Sphere();
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


class SSPTree {
private:
    SSPNode* _root;
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
    std::vector<int> kmeansClustering(const std::vector<Point>& points_to_cluster) {
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
        Point centroid1, centroid2;
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
                float dist_sq1 = Point::distance(points_to_cluster[i], centroid1);
                dist_sq1 *= dist_sq1;
                float dist_sq2 = Point::distance(points_to_cluster[i], centroid2);
                dist_sq2 *= dist_sq2;
                assignments[i] = (dist_sq1 < dist_sq2) ? 0 : 1;
            }

            // --- 2b. Paso de Actualización ---
            Point new_centroid1, new_centroid2;
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
            if (Point::distance(centroid1, new_centroid1) < CONVERGENCE_TOL &&
                Point::distance(centroid2, new_centroid2) < CONVERGENCE_TOL) {
                break; // Los centroides han convergido.
            }

            centroid1 = new_centroid1;
            centroid2 = new_centroid2;
        }

        return assignments;
    }

    SSPNode* chooseSubtree(SSPNode* node, const Point& point) {
        if (node->getIsLeaf()) return node;
        
        SSPNode* bestChild = nullptr;
        float minDistance = std::numeric_limits<float>::max();

        for (auto child : node->getChildren()) {
            float dist = Point::distance(child->getBoundingSphere().center, point);
            if (dist < minDistance) {
                minDistance = dist;
                bestChild = child;
            }
        }
        return bestChild;
    }
    
    void splitNode(SSPNode* node) {
        SSPNode* sibling = new SSPNode(node->getIsLeaf());
        std::vector<int> assignments;
        
        if (node->getIsLeaf()) { // --- División de un nodo hoja ---
            std::vector<Point> allPoints = node->_points;
            assignments = kmeansClustering(allPoints);
            
            // Distribuir puntos en los nodos según el clustering
            node->_points.clear();
            for (size_t i = 0; i < allPoints.size(); ++i) {
                if (assignments[i] == 0) node->_points.push_back(allPoints[i]);
                else sibling->_points.push_back(allPoints[i]);
            }

        } else { // --- División de un nodo interno ---
            std::vector<SSPNode*> allChildren = node->_children;
            std::vector<Point> centroids;
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
            SSPNode* newRoot = new SSPNode(false);
            newRoot->_children.push_back(node);
            newRoot->_children.push_back(sibling);
            node->setParent(newRoot);
            sibling->setParent(newRoot);
            newRoot->recalculateBoundingSphere();
            _root = newRoot;
        } else {
            SSPNode* parent = node->getParent();
            sibling->setParent(parent);
            parent->_children.push_back(sibling);
            
            // NOTA: No es necesario recalcular la esfera del padre aquí,
            // se hará en la propagación hacia arriba o en la llamada recursiva a splitNode.

            if (parent->getNumChildren() > _maxEntries) {
                splitNode(parent);
            } else {
                // Si el padre no se divide, tenemos que recalcular su esfera y propagar.
                 SSPNode* current = parent;
                 while(current) {
                    current->recalculateBoundingSphere();
                    current = current->getParent();
                 }
            }
        }
    }

    void adjustTree(SSPNode* node){
        // El ajuste se realiza en splitNode o en el bucle de inserción.
        // La propagación hacia arriba es clave.
        SSPNode* current = node;
        while(current != nullptr){
            current->recalculateBoundingSphere();
            if( (current->getIsLeaf() && current->getNumPoints() > _maxEntries) ||
                (!current->getIsLeaf() && current->getNumChildren() > _maxEntries) ){
                splitNode(current);
                // El split se encarga de su propio ajuste de padres.
                break;
            }
            current = current->getParent();
        }
    }

public:
    SSPTree() : _maxEntries(15), _root(nullptr) {}
    explicit SSPTree(std::size_t maxEntries) : _maxEntries(maxEntries), _root(nullptr) {}
    ~SSPTree() { delete _root; }

    SSPNode* getRoot() const { return _root; }

    void insert(const Point& point) {
        if (!_root) {
            _root = new SSPNode(true); // El primer nodo es una hoja
        }

        SSPNode* leaf = _root;
        while (!leaf->getIsLeaf()) {
            leaf = chooseSubtree(leaf, point);
        }
        
        leaf->_points.push_back(point);
        adjustTree(leaf);
    }

    bool search(const Point& point) const {
        if (!_root) return false;

        std::queue<SSPNode*> q;
        q.push(_root);
        
        while (!q.empty()) {
            SSPNode* current = q.front();
            q.pop();
            
            // Poda: si el punto no puede estar dentro de la esfera de contorno.
            if (Point::distance(current->getBoundingSphere().center, point) > current->getBoundingSphere().radius + EPSILON) {
                continue;
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    if (Point::distance(p, point) < EPSILON) {
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

    std::vector<Point> rangeQuery(const Sphere& sphere) const {
        std::vector<Point> result;
        if (!_root) return result;

        std::queue<SSPNode*> q;
        q.push(_root);

        while (!q.empty()) {
            SSPNode* current = q.front();
            q.pop();
            
            if (!current->getBoundingSphere().intersects(sphere)) {
                continue;
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    if (Point::distance(sphere.center, p) <= sphere.radius + EPSILON) {
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
    
    std::vector<Point> kNearestNeighbors(const Point& point, std::size_t k) const {
        std::vector<Point> result;
        if (!_root || k == 0) return result;
        
        // Usamos un max-heap para guardar los k vecinos más cercanos encontrados hasta ahora.
        // {distancia, Punto}
        using ResultElem = std::pair<float, Point>;
        auto cmp_Rel = [](const ResultElem& a, const ResultElem& b) { return a.first > b.first ? true : false; };
        std::priority_queue<ResultElem, std::vector<ResultElem>, decltype(cmp_Rel)> best_points(cmp_Rel);

        // Cola de prioridad para el recorrido del árbol (Best-First-Search)
        // Usamos un min-heap para explorar siempre el nodo más prometedor.
        // {distancia_al_nodo, SSPNode*}
        using QueueElem = std::pair<float, SSPNode*>;
        auto cmp_Qel = [](const QueueElem& a, const QueueElem& b) { return a.first > b.first ? true : false; };
        std::priority_queue<QueueElem, std::vector<QueueElem>, decltype(cmp_Qel)> pq(cmp_Qel);

        pq.push({0.0f, _root});

        while (!pq.empty()) {
            float dist_to_node_boundary = pq.top().first;
            SSPNode* current = pq.top().second;
            pq.pop();
            
            // Poda: si la distancia mínima al nodo es mayor que la distancia al k-ésimo vecino,
            // ningún punto en este subárbol puede ser un mejor vecino.
            if (best_points.size() == k && dist_to_node_boundary > best_points.top().first) {
                break; // Se puede podar el resto de la cola.
            }

            if (current->getIsLeaf()) {
                for (const auto& p : current->getPoints()) {
                    float dist = Point::distance(point, p);
                    if (best_points.size() < k) {
                        best_points.push({dist, p});
                    } else if (dist < best_points.top().first) {
                        best_points.pop();
                        best_points.push({dist, p});
                    }
                }
            } else {
                for (auto child : current->getChildren()) {
                    // Distancia desde el punto de consulta hasta el borde más cercano de la esfera del hijo
                    float dist_to_center = Point::distance(point, child->getBoundingSphere().center);
                    float dist_to_edge = std::max(0.0f, dist_to_center - child->getBoundingSphere().radius);
                    
                    // Poda: no añadas hijos que ya están más lejos que el k-ésimo vecino actual.
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
        // La priority_queue (max-heap) nos da los elementos de mayor a menor distancia,
        // así que los invertimos para tenerlos de más cercano a más lejano.
        std::reverse(result.begin(), result.end());
        return result;
    }
};

#endif // SSPTREE_H
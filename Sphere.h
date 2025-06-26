#ifndef SPHERE_H
#define SPHERE_H

#include "Point.h"
#include <algorithm> // Para std::max

struct Sphere {
    Point center;
    float radius;

    Sphere() 
      : center(), radius(0.0f) {}

    Sphere(const Point& c, float r)
      : center(c), radius(r) {}

    void expandToInclude(const Sphere& other) {
        float dist = Point::distance(center, other.center);

        // Si la otra esfera ya está contenida, no hay nada que hacer.
        if (dist + other.radius <= radius + EPSILON) {
            return;
        }
        
        // Si la esfera actual está contenida en la otra, simplemente la reemplazamos.
        if (dist + radius <= other.radius + EPSILON) {
            this->center = other.center;
            this->radius = other.radius;
            return;
        }

        // Calcula el nuevo radio y centro para la esfera combinada.
        float newRadius = (radius + dist + other.radius) / 2.0f;
        Point newCenter = center + (other.center - center) * ((newRadius - radius) / dist);
        
        this->center = newCenter;
        this->radius = newRadius;
    }

    void expandToInclude(const Point& p) {
        float dist = Point::distance(center, p);
        if (dist <= radius) {
            return; // El punto ya está dentro.
        }

        // El nuevo radio es la mitad de la distancia desde el borde opuesto de la esfera hasta el punto.
        float newRadius = (radius + dist) / 2.0f;
        // El nuevo centro se desplaza desde el centro antiguo hacia el punto.
        center += (p - center) * ((newRadius - radius) / dist);
        radius = newRadius;
    }
    
    bool intersects(const Sphere& other) const {
        return Point::distance(center, other.center) <= (radius + other.radius);
    }
};

#endif // SPHERE_H
#pragma once

#include <glm/glm.hpp>

namespace paracosm {

template <typename T>
class Cartographic {

public:

    Cartographic() { }
    Cartographic(T longitude, T latitude, T height) : longitude(longitude), latitude(latitude), height(height) { }

    union {
        struct {
            T longitude;
            T latitude;
            T height;
        };
        glm::vec<3, T, glm::highp> _data;
    };

};

}
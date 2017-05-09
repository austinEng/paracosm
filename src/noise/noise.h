#pragma once
namespace paracosm {
namespace noise {

template <unsigned int D, typename T>
class Noise {
public:
    virtual T sample(T[D] point) const = 0;
};

}
}
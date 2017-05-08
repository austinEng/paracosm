#pragma once
#include "noise.h"

namespace noise {

template <unsigned int D, typename T>
class MultiOctaveValueNoise : public Noise<D, T> {
public:

    typedef struct Config {
        T baseWavelength;
        T baseFrequency;
        T persistence;
        T maxAmplitude;
    } Config;

    MultiOctaveValueNoise(const Config &config) : config(config) { }

    virtual T sample(T[D] point) const {
        return 0;
    }

private:
    const Config config;

};

}
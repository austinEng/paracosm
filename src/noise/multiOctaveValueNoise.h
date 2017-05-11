#pragma once

namespace paracosm {
namespace noise {

template <unsigned int D, typename T>
class MultiOctaveValueNoise {
public:

    typedef struct Config {
        T baseWavelength;
        T baseFrequency;
        T persistence;

        Config() {}
        Config(T baseWavelength, T baseFrequency, T persistence) : baseWavelength(baseWavelength), baseFrequency(baseFrequency), persistence(persistence) {}
    } Config;

    MultiOctaveValueNoise(const Config &config) : config(config) { }
    
    T sample(T point[D], unsigned int octaves) const {
        T total = 0;

        unsigned int sampleCount = (unsigned int) std::pow(2, D);
        T* samples = new T[sampleCount];

        T frequency = config.baseFrequency;
        T amplitude = 1;
        for (unsigned int i = 0; i < octaves; ++i) {
        
            int integralPart[D];
            T fractionalPart[D];
            for (unsigned int d = 0; d < D; ++d) {
                T samplePoint = point[d] * frequency / config.baseWavelength;
                integralPart[d] = (int) std::floor(samplePoint);
                fractionalPart[d] = samplePoint - integralPart[d];
            }

            for (unsigned int s = 0; s < sampleCount; ++s) {
                int samplePoint[D];
                for (unsigned int d = 0, mask = 1; d < D; ++d, mask <<= 1) {
                    samplePoint[d] = (s & mask) + integralPart[d];
                }
                samples[s] = 2 * (noise(samplePoint) - 0.5);
            }
            
            for (unsigned int d = 0, offset = 1; d < D; ++d, offset *= 2) {
                for (unsigned int s = 0; s < sampleCount; s += 2*offset) {
                    samples[s] = cosineInterpolate(samples[s], samples[s + offset], fractionalPart[d]);
                }
            }

            total += amplitude * samples[0];

            frequency *= 2;
            amplitude *= config.persistence;
        }

        delete samples;

        return total;
    }

private:
    const Config config;

    static T noise(int point[D]) {
        static T seeds[9] = {12.989, 78.2342, 352.5345, 8448.56, 389.335, 5232.545, 23.43243, 234.4347, 84.435};

        T val = 0;
        for (unsigned int d = 0; d < D; ++d) {
            val += point[d] * seeds[d];
        }
        val = std::sin(val) * 43758.5453;

        return val - std::floor(val);
    }

    static inline T interpolate(T a, T b, T t) {
        return a * (1 - t) + b * t;
    }

    static inline T cosineInterpolate(T a, T b, T t) {
        t = (1 - std::cos(t * M_PI)) * 0.5;
        return interpolate(a, b, t);
    }

};

}
}

#pragma once

#include <json/json.h>

enum Hemisphere {
    WEST = 0,
    EAST = 1
};

struct BoundingRegion {
    double w;
    double s;
    double e;
    double n;
    double h1;
    double h2;

    BoundingRegion(double w, double s, double e, double n, double h1 = 0, double h2 = 0) : w(w), s(s), e(e), n(n), h1(h1), h2(h2) { }
};

class TerrainGenerator {

public:

    struct Config {
        double maximumDisplacement;
        double persistence;
        double levelDisplacement;
        unsigned int generationDepth;
        unsigned int contentGenerationDepth;
        double ellipsoid[3];

        void computeProperties();
    };

    const Config config;

    TerrainGenerator(const Config &config);
    ~TerrainGenerator();

    BoundingRegion getBoundingTile(Hemisphere, unsigned int index, unsigned int &depth) const;

    BoundingRegion generateBoundingRegion(Hemisphere hemisphere, unsigned int index, double &terrainError) const;

    unsigned int getDepth(unsigned int index) const;

    double sampleHeight(double longitude, double latitude, unsigned int level) const;

    double calculateRegionError(const BoundingRegion &region) const;

    double calculateTerrainError(unsigned int level, unsigned int depth) const;

    char* generateTerrain(Hemisphere hemisphere, unsigned int index, size_t &length) const;

private:
    Json::Value baseTerrain;
};
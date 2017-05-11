
#include <stdlib.h>
#include <stdint.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>
#include <functional>
#include <vector>
#include <iostream>
#include "terrainGenerator.h"

namespace paracosm {

const BoundingRegion ROOT_REGIONS[2] = {
    BoundingRegion(-M_PI, -M_PI / 2, 0, M_PI / 2),
    BoundingRegion(0, -M_PI / 2, M_PI, M_PI / 2)
};

void TerrainGenerator::Config::computeProperties() {
    levelDisplacement = -maximumDisplacement / std::log(persistence);
}

TerrainGenerator::TerrainGenerator(const Config &config) : 
    config(config) {

    Json::Reader reader;

    static const char* baseTerrainJson = 
    #include "baseTerrain.gltf"
    
    bool success = reader.parse(baseTerrainJson, baseTerrain, false);
    if (!success) {
        std::cout  << "Failed to parse baseTerrain.gltf" << reader.getFormattedErrorMessages() << std::endl;
        exit(EXIT_FAILURE);
    }
}

TerrainGenerator::~TerrainGenerator() { }

const std::function<void(BoundingRegion&)> regionModifiers[4] = {
    // south west
    [](BoundingRegion &region) {
        region.e = (region.w + region.e) / 2;
        region.n = (region.s + region.n) / 2;
    },
    // south east
    [](BoundingRegion &region) {
        region.w = (region.w + region.e) / 2;
        region.n = (region.s + region.n) / 2;
    },
    // north east
    [](BoundingRegion &region) {
        region.w = (region.w + region.e) / 2;
        region.s = (region.s + region.n) / 2;
    },
    // north west
    [](BoundingRegion &region) {
        region.e = (region.w + region.e) / 2;
        region.s = (region.s + region.n) / 2;
    }
};

BoundingRegion TerrainGenerator::getBoundingTile(Hemisphere hemisphere, unsigned int index, unsigned int &depth) const {
    depth = getDepth(index);

    BoundingRegion region = ROOT_REGIONS[hemisphere];

    std::vector<unsigned int> indices;
    while (index > 0) {
        unsigned int next = (index - 1) / 4;
        unsigned int childIndex = index - next * 4;
        indices.push_back(childIndex - 1);
        index = next;
    }

    for (auto it = indices.rbegin(); it != indices.rend(); ++it) {
        regionModifiers[*it](region);
    }

    return region;
}

BoundingRegion TerrainGenerator::generateBoundingRegion(Hemisphere hemisphere, unsigned int index, double &terrainError) const {
    unsigned int depth;
    BoundingRegion region = getBoundingTile(hemisphere, index, depth);

    double sw = sampleHeight(region.w, region.s, depth);
    double nw = sampleHeight(region.w, region.n, depth);
    double se = sampleHeight(region.e, region.s, depth);
    double ne = sampleHeight(region.e, region.n, depth);
    terrainError = calculateTerrainError(depth, config.contentGenerationDepth);
    
    region.h1 = std::min(std::min(sw, nw), std::min(se, ne)) - terrainError;
    region.h2 = std::max(std::max(sw, nw), std::max(se, ne)) + terrainError;
    
    return region;
}

unsigned int TerrainGenerator::getDepth(unsigned int index) const {
    unsigned int depth = 0;
    while (index > 0) {
        ++depth;
        index = (index + 3) / 4 - 1;
    }
    return depth;
}

double TerrainGenerator::sampleHeight(double longitude, double latitude, unsigned int level) const {
    noise::MultiOctaveValueNoise<2, double> noiseGenerator(noise::MultiOctaveValueNoise<2, double>::Config(2*M_PI, 1, config.persistence));
    double point[2] = {longitude, latitude};
    return config.levelDisplacement * noiseGenerator.sample(point, level);
}

template <typename A, typename B, typename C, typename D>
void cartographicToCartesian(A cartographic[3], B ellipsoid[3], C cartesian[3], D normal[3]) {
    double cosLatitude = std::cos(cartographic[1]);
    double nx = cosLatitude * std::cos(cartographic[0]);
    double ny = cosLatitude * std::sin(cartographic[0]);
    double nz = std::sin(cartographic[1]);
    double length = std::sqrt(nx*nx + ny*ny + nz*nz);
    nx /= length;
    ny /= length;
    nz /= length;

    normal[0] = (D) nx;
    normal[1] = (D) ny;
    normal[2] = (D) nz;

    double kx = nx * ellipsoid[0] * ellipsoid[0];
    double ky = ny * ellipsoid[1] * ellipsoid[1];
    double kz = nz * ellipsoid[2] * ellipsoid[2];

    double gamma = std::sqrt(nx * kx + ny * ky + nz * kz);
    kx /= gamma;
    ky /= gamma;
    kz /= gamma;

    nx *= cartographic[2];
    ny *= cartographic[2];
    nz *= cartographic[2];

    cartesian[0] = (C) (nx + kx);
    cartesian[1] = (C) (ny + ky);
    cartesian[2] = (C) (nz + kz);
}

double TerrainGenerator::calculateRegionError(const BoundingRegion &region) const {
    double cartographic[3] = {0,0,0};
    double cartesian[3];
    double normal[3];

    double radius = 0;

    cartographic[0] = region.w;
    cartographic[1] = region.s;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += std::sqrt(cartesian[0] * cartesian[0] + cartesian[1] * cartesian[1] + cartesian[2] * cartesian[2]);

    cartographic[0] = region.e;
    cartographic[1] = region.s;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += std::sqrt(cartesian[0] * cartesian[0] + cartesian[1] * cartesian[1] + cartesian[2] * cartesian[2]);

    cartographic[0] = region.w;
    cartographic[1] = region.n;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += std::sqrt(cartesian[0] * cartesian[0] + cartesian[1] * cartesian[1] + cartesian[2] * cartesian[2]);

    cartographic[0] = region.e;
    cartographic[1] = region.n;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += std::sqrt(cartesian[0] * cartesian[0] + cartesian[1] * cartesian[1] + cartesian[2] * cartesian[2]);

    radius /= 4;

    // the arc this region spans
    double theta = (region.e - region.w) / std::pow(2, config.contentGenerationDepth);

    // how far off the chord spanning this arc is from the arc
    double chordError = radius * (1 - std::cos(theta / 2));

    return chordError;
}

double TerrainGenerator::calculateTerrainError(unsigned int level, unsigned int depth) const {
    double oneOverPersistence = 1 / config.persistence;

    // integral of a^x from b to c = (a^(c) - a^(b)) / log(a)
    return config.levelDisplacement * (std::pow(config.persistence, level + depth) - std::pow(config.persistence, level)) / std::log(config.persistence);
}

char* TerrainGenerator::generateTerrain(Hemisphere hemisphere, unsigned int index, size_t &length) const {
    unsigned int depth;
    BoundingRegion region = getBoundingTile(hemisphere, index, depth);
    double step = std::pow(0.5, config.contentGenerationDepth);
    unsigned int steps = (unsigned int) std::pow(2, config.contentGenerationDepth);

    unsigned int indexCount = 6 * steps * steps;
    unsigned int vertexCount = (steps + 1) * (steps + 1);

    unsigned int indicesSize = sizeof(uint16_t) * indexCount;
    unsigned int positionsSize = sizeof(float) * 3 * vertexCount;
    unsigned int normalsSize = sizeof(float) * 3 * vertexCount;
    unsigned int uvsSize = sizeof(float) * 2 * vertexCount;
    
    unsigned int indicesOffset = 0;
    unsigned int positionsOffset = indicesOffset + indicesSize;
    unsigned int normalsOffset = positionsOffset + positionsSize;
    unsigned int uvsOffset = normalsOffset + normalsSize;

    uint32_t bufferLength = indicesSize + positionsSize + normalsSize + uvsSize;
    char* glbBuffer = new char[bufferLength];
    uint16_t* indices = (uint16_t*) (glbBuffer + indicesOffset);
    float* positions = (float*) (glbBuffer + positionsOffset);
    float* normals = (float*) (glbBuffer + normalsOffset);
    float* uvs = (float*) (glbBuffer + uvsOffset);

    float minPosition[3];
    float maxPosition[3];
    unsigned int idx = 0;
    for (unsigned int i = 0; i <= steps; ++i) {
        double longitude = region.w + (i * step) * (region.e - region.w);
        for (unsigned int j = 0; j <= steps; ++j) {
            double latitude = region.s + (j * step) * (region.n - region.s);
            double height = sampleHeight(longitude, latitude, depth + config.contentGenerationDepth);
            
            double cartographic[3] = {longitude, latitude, height};
            
            cartographicToCartesian(cartographic, config.ellipsoid, positions + 3*idx, normals + 3*idx);

            if (i == 0 && j == 0) {
                minPosition[0] = positions[3*idx + 0];
                minPosition[1] = positions[3*idx + 1];
                minPosition[2] = positions[3*idx + 2];
                maxPosition[0] = positions[3*idx + 0];
                maxPosition[1] = positions[3*idx + 1];
                maxPosition[2] = positions[3*idx + 2];
            } else {
                minPosition[0] = std::min(minPosition[0], positions[3*idx + 0]);
                minPosition[1] = std::min(minPosition[1], positions[3*idx + 1]);
                minPosition[2] = std::min(minPosition[2], positions[3*idx + 2]);
                maxPosition[0] = std::max(maxPosition[0], positions[3*idx + 0]);
                maxPosition[1] = std::max(maxPosition[1], positions[3*idx + 1]);
                maxPosition[2] = std::max(maxPosition[2], positions[3*idx + 2]);
            }

            uvs[2*idx + 0] = (float) i / steps;
            uvs[2*idx + 1] = (float) j / steps;

            ++idx;
        }
    }

    idx = 0;
    for (unsigned int i = 0; i < steps; ++i) {
        for (unsigned int j = 0; j < steps; ++j) {
            indices[3*idx + 0] = i * (steps + 1) + j;
            indices[3*idx + 1] = (i + 1) * (steps + 1) + j;
            indices[3*idx + 2] = (i + 1) * (steps + 1) + j + 1;
            ++idx;

            indices[3*idx + 0] = i * (steps + 1) + j;
            indices[3*idx + 1] = (i + 1) * (steps + 1) + j + 1;
            indices[3*idx + 2] = i * (steps + 1) + j + 1;
            ++idx;
        }
    }

    Json::Value terrain = baseTerrain;
    terrain["accessors"]["accessor_ind"]["count"] = indexCount;
    terrain["accessors"]["accessor_pos"]["count"] = vertexCount;
    terrain["accessors"]["accessor_nor"]["count"] = vertexCount;
    terrain["accessors"]["accessor_uv"]["count"] = vertexCount;
    
    terrain["accessors"]["accessor_pos"]["min"][0] = minPosition[0];
    terrain["accessors"]["accessor_pos"]["min"][1] = minPosition[1];
    terrain["accessors"]["accessor_pos"]["min"][2] = minPosition[2];
    terrain["accessors"]["accessor_pos"]["max"][0] = maxPosition[0];
    terrain["accessors"]["accessor_pos"]["max"][1] = maxPosition[1];
    terrain["accessors"]["accessor_pos"]["max"][2] = maxPosition[2];

    terrain["accessors"]["accessor_pos"]["byteOffset"] = 0;
    terrain["accessors"]["accessor_nor"]["byteOffset"] = 3 * sizeof(float) * vertexCount;
    terrain["accessors"]["accessor_uv"]["byteOffset"] = (3 + 3) * sizeof(float) * vertexCount;

    terrain["bufferViews"]["bufferView_ind"]["byteLength"] = indicesSize;
    terrain["bufferViews"]["bufferViews_attr"]["byteLength"] = positionsSize + normalsSize + uvsSize;

    terrain["bufferViews"]["bufferView_ind"]["byteOffset"] = 0;
    terrain["bufferViews"]["bufferViews_attr"]["byteOffset"] = indicesSize;

    terrain["buffers"]["binary_glTF"]["byteLength"] = indicesSize + positionsSize + normalsSize + uvsSize;

    Json::FastWriter writer;
    std::string json = writer.write(terrain);

    const uint32_t b3dmHeaderLength = 28;
    const uint32_t featureTableLength = 0;
    const uint32_t batchTableLength = 0;
    const uint32_t gltfHeaderLength = 20;
    uint32_t jsonLength = (uint32_t) json.length();
    uint32_t contentLength = ((jsonLength + 3) / 4) * 4; // pad by 4
    uint32_t glbLength = gltfHeaderLength + contentLength + bufferLength;
    uint32_t featureTableJsonLength = ((0 + 7) / 8) * 8;
    uint32_t featureTableBinaryLength = ((0 + 7) / 8) * 8;
    uint32_t batchTableJsonLength = ((0 + 7) / 8) * 8;
    uint32_t batchTableBinaryLength = ((0 + 7) / 8) * 8;

    length = b3dmHeaderLength + featureTableLength + batchTableLength + glbLength;
    char* data = new char[length];

    memcpy(data, "b3dm", 4);
    *(uint32_t*)(data + 4*1) = 1;
    *(uint32_t*)(data + 4*2) = (uint32_t) length;
    *(uint32_t*)(data + 4*3) = featureTableJsonLength;
    *(uint32_t*)(data + 4*4) = featureTableBinaryLength;
    *(uint32_t*)(data + 4*5) = batchTableJsonLength;
    *(uint32_t*)(data + 4*6) = batchTableBinaryLength;

    char* glb = data + b3dmHeaderLength + featureTableLength + batchTableLength;

    memcpy(glb, "glTF", 4);
    *(uint32_t*)(glb + 4*1) = 1;
    *(uint32_t*)(glb + 4*2) = glbLength;
    *(uint32_t*)(glb + 4*3) = contentLength;
    *(uint32_t*)(glb + 4*4) = 0; // contentFormat
    memcpy(glb + 4*5, json.c_str(), jsonLength);
    memset(glb + 4*5 + jsonLength, ' ', contentLength - jsonLength); // fill the padding with spaces
    
    memcpy(glb + gltfHeaderLength + contentLength, glbBuffer, bufferLength);
    delete glbBuffer;

    return data;
}

}
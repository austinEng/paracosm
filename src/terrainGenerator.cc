
#include <stdlib.h>
#include <stdint.h>
#include <cstring>
#define _USE_MATH_DEFINES
#include <cmath>
#include <algorithm>
#include <functional>
#include <vector>
#include <iostream>
#include "terrainGenerator.h"
#include "cartographic.h"

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

BoundingRegion TerrainGenerator::generateBoundingRegion(Hemisphere hemisphere, unsigned int index) const {
    unsigned int depth;
    BoundingRegion region = getBoundingTile(hemisphere, index, depth);

    double sw = sampleHeight(region.w, region.s, depth);
    double nw = sampleHeight(region.w, region.n, depth);
    double se = sampleHeight(region.e, region.s, depth);
    double ne = sampleHeight(region.e, region.n, depth);
    double error = calculateErrorDifference(depth, depth + config.contentGenerationDepth);
    
    region.h1 = std::min(std::min(sw, nw), std::min(se, ne)) - error;
    region.h2 = std::max(std::max(sw, nw), std::max(se, ne)) + error;
    
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

template <typename A, typename B, typename C, typename D>
void cartographicToCartesian(const Cartographic<A> &cartographic, const glm::tvec3<B> &ellipsoid, glm::tvec3<C> &cartesian, glm::tvec3<D> &normal) {
    double cosLatitude = std::cos(cartographic.latitude);
    normal.x = (D) cosLatitude * std::cos(cartographic.longitude);
    normal.y = (D) cosLatitude * std::sin(cartographic.longitude);
    normal.z = (D) std::sin(cartographic.latitude);
    normal = glm::normalize(normal);

    glm::tvec3<D> k(
        normal.x * ellipsoid.x * ellipsoid.x,
        normal.y * ellipsoid.y * ellipsoid.y,
        normal.z * ellipsoid.z * ellipsoid.z
    );

    double gamma = std::sqrt(glm::dot(normal, k));
    k /= gamma;

    cartesian.x = normal.x * cartographic.height + k.x;
    cartesian.y = normal.y * cartographic.height + k.y;
    cartesian.z = normal.z * cartographic.height + k.z;
}

double TerrainGenerator::sampleHeight(double longitude, double latitude, unsigned int level) const {
    Cartographic<double> cartographic(longitude, latitude, 0);
    glm::dvec3 point;
    static glm::dvec3 scratchNormal;
    static glm::dvec3 ellipsoid(0.5, 0.5, 0.5);

    cartographicToCartesian(cartographic, ellipsoid, point, scratchNormal);

    noise::MultiOctaveValueNoise<3, double> noiseGenerator(noise::MultiOctaveValueNoise<3, double>::Config(1, 1, config.persistence));
    return config.levelDisplacement * noiseGenerator.sample(&point[0], level);
}

double TerrainGenerator::calculateRegionError(const BoundingRegion &region) const {
    Cartographic<double> cartographic(0, 0, 0);
    glm::dvec3 cartesian;
    glm::dvec3 normal;

    double radius = 0;

    cartographic.longitude = region.w;
    cartographic.latitude = region.s;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += glm::length(cartesian);

    cartographic.longitude = region.e;
    cartographic.latitude = region.s;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += glm::length(cartesian);

    cartographic.longitude = region.w;
    cartographic.latitude = region.n;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += glm::length(cartesian);

    cartographic.longitude = region.e;
    cartographic.latitude = region.n;
    cartographicToCartesian(cartographic, config.ellipsoid, cartesian, normal);
    radius += glm::length(cartesian);

    radius /= 4;

    // the arc this region spans
    double theta = (region.e - region.w) / std::pow(2, config.contentGenerationDepth);

    // how far off the chord spanning this arc is from the arc
    double chordError = radius * (1 - std::cos(theta / 2));

    return chordError;
}

double TerrainGenerator::calculateErrorDifference(unsigned int levelA, unsigned int levelB) const {
    // integral of a^x from b to c = (a^(c) - a^(b)) / log(a)
    return config.levelDisplacement * (std::pow(config.persistence, levelB) - std::pow(config.persistence, levelA)) / std::log(config.persistence);
}

double TerrainGenerator::calculateRemainingError(unsigned int level) const {
    return config.levelDisplacement * -std::pow(config.persistence, level) / std::log(config.persistence);
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

    glm::vec3 minPosition;
    glm::vec3 maxPosition;
    unsigned int idx = 0;
    for (unsigned int i = 0; i <= steps; ++i) {
        double longitude = region.w + (i * step) * (region.e - region.w);
        for (unsigned int j = 0; j <= steps; ++j) {
            double latitude = region.s + (j * step) * (region.n - region.s);
            double height = sampleHeight(longitude, latitude, depth + config.contentGenerationDepth);
            
            Cartographic<double> cartographic(longitude, latitude, height);
            
            glm::vec3& position = *(glm::vec3*)(positions + 3*idx);
            glm::vec3& normal = *(glm::vec3*)(normals + 3*idx);
            cartographicToCartesian(cartographic, config.ellipsoid, position, normal);

            if (i == 0 && j == 0) {
                minPosition = position;
                maxPosition = position;
            } else {
                minPosition = glm::min(minPosition, position);
                maxPosition = glm::max(maxPosition, position);
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
    terrain["accessors"]["accessor_nor"]["byteOffset"] = (unsigned int)(3 * sizeof(float) * vertexCount);
    terrain["accessors"]["accessor_uv"]["byteOffset"] = (unsigned int)((3 + 3) * sizeof(float) * vertexCount);

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

    std::memcpy(data, "b3dm", 4);
    *(uint32_t*)(data + 4*1) = 1;
    *(uint32_t*)(data + 4*2) = (uint32_t) length;
    *(uint32_t*)(data + 4*3) = featureTableJsonLength;
    *(uint32_t*)(data + 4*4) = featureTableBinaryLength;
    *(uint32_t*)(data + 4*5) = batchTableJsonLength;
    *(uint32_t*)(data + 4*6) = batchTableBinaryLength;

    char* glb = data + b3dmHeaderLength + featureTableLength + batchTableLength;

    std::memcpy(glb, "glTF", 4);
    *(uint32_t*)(glb + 4*1) = 1;
    *(uint32_t*)(glb + 4*2) = glbLength;
    *(uint32_t*)(glb + 4*3) = contentLength;
    *(uint32_t*)(glb + 4*4) = 0; // contentFormat
    std::memcpy(glb + 4*5, json.c_str(), jsonLength);
    std::memset(glb + 4*5 + jsonLength, ' ', contentLength - jsonLength); // fill the padding with spaces
    
    std::memcpy(glb + gltfHeaderLength + contentLength, glbBuffer, bufferLength);
    delete glbBuffer;

    return data;
}

}
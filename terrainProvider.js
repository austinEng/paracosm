'use strict';

const Cesium = require('cesium');
const gltfPipeline = require('gltf-pipeline');
const glbToB3dm = require('3d-tiles-tools/lib/glbToB3dm');
const addPipelineExtras = gltfPipeline.addPipelineExtras;
const getBinaryGltf = gltfPipeline.getBinaryGltf;
const loadGltfUris = gltfPipeline.loadGltfUris;
const processJSON = gltfPipeline.Pipeline.processJSON;
const addCesiumRTC = gltfPipeline.addCesiumRTC;

//TERRAIN GENERATION STUFF
const THREE = require('three');

//NOISE GENERATION
function getFractionalPart(a)
{
    if(a < 0)
        {
            return -(Math.abs(a) - Math.floor(Math.abs(a)));
        }
    else if(a > 0)
        {
            return a - Math.floor(a);
        }
    else if(a == 0)
        return 0;
    
}

function getIntigerPart(a)
{
    if(a < 0)
        {
            return -(Math.floor(Math.abs(a)));
        }
    else if(a > 0)
        {
            return Math.floor(a);
        }
    else if(a == 0)
        return 0;
}

function dotProduct(x1, y1, z1, x2, y2, z2)
{
    var dotproduct = ((x1 * x2) + (y1 * y2) + (z1 * z2));
    return dotproduct;
}

function Noise3D(x, y, z)
{
    var ft = Math.sin(dotProduct(x,y,z, 12.989, 78.233, 157)) * 43758.5453;
    
    return getFractionalPart(ft);
}


function SmoothNoise3D(X, Y, Z)
{
    var far = (Noise3D(X-1, Y+1, Z+1) + Noise3D(X+1, Y+1, Z+1) + Noise3D(X-1, Y+1, Z-1) + Noise3D(X+1, Y+1, Z-1) + Noise3D(X-1, Y-1, Z+1) + Noise3D(X+1, Y-1, Z+1) + Noise3D(X-1, Y-1, Z-1) + Noise3D(X+1, Y-1, Z-1)) / 64.0;//80.0;

    var medium = (Noise3D(X-1, Y+1, Z) + Noise3D(X+1, Y+1, Z) + Noise3D(X-1, Y-1, Z) + Noise3D(X+1, Y-1, Z) + Noise3D(X, Y+1, Z+1) + Noise3D(X, Y+1, Z-1) + Noise3D(X, Y-1, Z+1) + Noise3D(X, Y-1, Z-1) + Noise3D(X-1, Y, Z+1) + Noise3D(X+1, Y, Z+1) + Noise3D(X-1, Y, Z-1) + Noise3D(X+1, Y, Z-1)) / 32.0;//60.0;

    var closest = (Noise3D(X-1, Y, Z) + Noise3D(X+1, Y, Z) + Noise3D(X, Y-1, Z) + Noise3D(X, Y+1, Z) + Noise3D(X, Y, Z+1) + Noise3D(X, Y, Z-1)) / 16.0;//19.999;
    
    var self = Noise3D(X, Y, Z) / 8.0;
    
    
    return self + closest + medium + far;  
}


function Interpolate(a, b, x)
{
    var t = (1.0 - Math.cos(x * 3.14159)) * 0.5;
    
    return a * (1.0 - t) + b * t;
}

function InterpolateNoise3D(x, y, z, p)
{
    var int_X = getIntigerPart(x);
    var int_Y = getIntigerPart(y);
    var int_Z = getIntigerPart(z);
    
    var float_X = getFractionalPart(x);
    var float_Y = getFractionalPart(y);
    var float_Z = getFractionalPart(z);
    
    //8 Points on the lattice sorrunding the given point
    var p1 = SmoothNoise3D(int_X, int_Y, int_Z);
    var p2 = SmoothNoise3D(int_X + 1, int_Y, int_Z);
    var p3 = SmoothNoise3D(int_X, int_Y + 1, int_Z);
    var p4 = SmoothNoise3D(int_X + 1, int_Y + 1, int_Z);
    var p5 = SmoothNoise3D(int_X, int_Y, int_Z + 1);
    var p6 = SmoothNoise3D(int_X + 1, int_Y, int_Z + 1);
    var p7 = SmoothNoise3D(int_X, int_Y + 1, int_Z + 1);
    var p8 = SmoothNoise3D(int_X + 1, int_Y + 1, int_Z + 1);
    
    var i1 = Interpolate(p1, p2, float_X);
    var i2 = Interpolate(p3, p4, float_X);
    var i3 = Interpolate(p5, p6, float_X);
    var i4 = Interpolate(p7, p8, float_X);
    
    var n1 = Interpolate(i1, i2, float_Y);
    var n2 = Interpolate(i3, i4, float_Y);
    
    var t1 = Interpolate(n1, n2, float_Z);
    
    return t1;
}


function Generate_Noise3D(posx, posy, posz, persistance, octaves)
{
    var total = 0.0;
    var p = 0.25;//persistance;
    var n = octaves;

    //int i = 0;
    for(var i=0; i < octaves; i++) 
    {
    var frequency = Math.pow(1/p, i);//Math.pow(2, i);/// 120000000;//12000000.0;
    var amplitude = Math.pow(p, i*i);
    
    total = total + InterpolateNoise3D((posx)* frequency * 100, (posy) * frequency * 100, (posz) * frequency * 100, amplitude) * amplitude;
    
    }
    
    return total;
}


//NOISE GENERATION OVER

//GETTING THE TREE DEPTH
function getDepth(index) {
  var depth = 0;
  while (index > 0) {
    ++depth;
    index = Math.ceil(index / 4) - 1;
  }
  return depth; //depth / 100
}

//SETTING UP THE POS AND NORMAL VECTORS FROM RADIANS TO CARTESIAN COORDINATES
function TerrainProvider(treeProvider) {
    this.treeProvider = treeProvider;
}


var scratchCartographic = new Cesium.Cartographic();
var scratchCartesian = new Cesium.Cartesian3();

function setPosition(originlon, originlat, lon, lat, height, result) {
    Cesium.Cartographic.fromRadians(lon, lat, height, scratchCartographic);
    Cesium.Ellipsoid.WGS84.cartographicToCartesian(scratchCartographic, scratchCartesian);
    result[0] = scratchCartesian.x;
    result[1] = scratchCartesian.y;
    result[2] = scratchCartesian.z;


    // position is relative to southwest corner
    //  Cesium.Cartographic.fromRadians(originlon, originlat, 0, scratchCartographic);
    //  Cesium.Ellipsoid.WGS84.cartographicToCartesian(scratchCartographic, scratchCartesian);
    //  result[0] -= scratchCartesian.x;
    //  result[1] -= scratchCartesian.y;
    //  result[2] -= scratchCartesian.z;


}

function setNormal(lon, lat, height, result) {
    Cesium.Cartographic.fromRadians(lon, lat, height, scratchCartographic);
    Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(scratchCartographic, scratchCartesian);
    result[0] = scratchCartesian.x;
    result[1] = scratchCartesian.y;
    result[2] = scratchCartesian.z;
}

//set normal from cartesian
var normal = [];
function setcartesiannormal(posx, posy, posz) {
    normal = [];
    
    //convert cartesian positions to cartographics on a WGS84 ellpisoid
    var position = new Cesium.Cartesian3(posx, posy, posz);
    var cartographicPosition = Cesium.Ellipsoid.WGS84.cartesianToCartographic(position);
    
    //convert cartographics rep. to normal rep. on the ellipsoid
    var normalCartesian = new Cesium.Cartesian3();
    Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(cartographicPosition, normalCartesian);

    normal[0] = normalCartesian.x;
    normal[1] = normalCartesian.y;
    normal[2] = normalCartesian.z;
    
}

//setting up the subdivision row positions
var pos_arr = [];
function getRowSubdivPos(depth, pointl, pointr, indexl, indexr) 
{
    if(depth > 2)
    {
        return;
    }

    pos_arr[(indexl + indexr) / 2] = new THREE.Vector3((pointr.x + pointl.x) / 2, (pointr.y + pointl.y) / 2, (pointr.z + pointl.z) / 2);
    
    depth += 1;
    getRowSubdivPos(depth, pointl, pos_arr[(indexl + indexr) / 2], indexl, (indexl + indexr) / 2);
    getRowSubdivPos(depth, pos_arr[(indexl + indexr) / 2], pointr, (indexl + indexr) / 2, indexr);

}

TerrainProvider.prototype.generateTerrain = function (hemisphere, index) {
    // west, south, east, north
    var region = this.treeProvider.generateBoundingRegion(hemisphere, index);

    
    var northPole = (region[3] === Math.PI / 2); //false
    var southPole = (region[1] === -Math.PI / 2); //false
    var pole = northPole || southPole;

//    var vertexCount = pole ? 3 : 9;//81;//9;//4;
//    var indexCount = 3 * (vertexCount - 2);

//    var indices
//    if(pole)
//        
//    else
//        indices = new Uint16Array(24);//384);
    var vertexCount, indexCount;
    
    if (pole) {
        vertexCount = 3;
        indexCount = 3;
    } else {
        vertexCount = 81;//9
        indexCount = 384;//24;
    }
    
    var indices = new Uint16Array(indexCount);
    
    var normals = new Float32Array(3 * vertexCount);
    var positions = new Float32Array(3 * vertexCount);

    var componentBytes = 3 * Float32Array.BYTES_PER_ELEMENT;
    var corner;


    if (!pole) 
    {
        // west, south, east, north
        var SUBS = 3;

        var stepLong = (region[2] - region[0]) * Math.pow(0.5, SUBS); //left to right
        var stepLat = (region[3] - region[1]) * Math.pow(0.5, SUBS); //bottom to top
        
        var CartographicPos = new Cesium.Cartographic();
        var CartesianPos = new Cesium.Cartesian3();
        
        var p = 0;
        for (var i = 0; i <= Math.pow(2,SUBS); ++i) 
        {
            for (var j = 0; j <= Math.pow(2,SUBS); ++j) 
            {
                var lat = region[1] + i * stepLat;
                var long = region[0] + j * stepLong;
                
                //set the position
                Cesium.Cartographic.fromRadians(long, lat, 0, CartographicPos);
                Cesium.Ellipsoid.WGS84.cartographicToCartesian(CartographicPos, CartesianPos);
                
                positions[3*p+0] = CartesianPos.x;
                positions[3*p+1] = CartesianPos.y;
                positions[3*p+2] = CartesianPos.z;
            
                //set the normal
                Cesium.Cartographic.fromRadians(long, lat, 0, CartographicPos);
                Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(CartographicPos, CartesianPos);
                
                normals[3*p+0] = CartesianPos.x;
                normals[3*p+1] = CartesianPos.y;
                normals[3*p+2] = CartesianPos.z;
                
                //OFFSET THE POSITIONS OF THE VERTICES ALONG THE NORMAL USING NOISE
                var tree_depth = getDepth(index);
                var noise_val = Generate_Noise3D(positions[3*p+0], positions[3*p+1], positions[3*p+2], 0.9, tree_depth);
                
                noise_val = 0.5 + 0.5 * noise_val;
                positions[3*p+0] = positions[3*p+0] + normals[3*p+0] * noise_val * 2000000;
                positions[3*p+1] = positions[3*p+1] + normals[3*p+1] * noise_val * 2000000;
                positions[3*p+2] = positions[3*p+2] + normals[3*p+2] * noise_val * 2000000;
                
                p+=1;
            }
        }
    }
    else
    {
        //  0      1     2     3  
        // west, south, east, north
        //set position of the triangle based in the south pole or the north pole
        var CartographicPos = new Cesium.Cartographic();
        var CartesianPos = new Cesium.Cartesian3();
        var p = 0;
        
        //south west
        //set position
        Cesium.Cartographic.fromRadians(region[0], region[1], 0, CartographicPos);
        Cesium.Ellipsoid.WGS84.cartographicToCartesian(CartographicPos, CartesianPos);
        
        positions[3*p+0] = CartesianPos.x;
        positions[3*p+1] = CartesianPos.y;
        positions[3*p+2] = CartesianPos.z;
        
        
        //set the normal
        Cesium.Cartographic.fromRadians(region[0], region[1], 0, CartographicPos);
        Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(CartographicPos, CartesianPos);

        normals[3*p+0] = CartesianPos.x;
        normals[3*p+1] = CartesianPos.y;
        normals[3*p+2] = CartesianPos.z;
        p++;
        
        if(!southPole)
        {
            //south east
            //set position
            Cesium.Cartographic.fromRadians(region[2], region[1], 0, CartographicPos);
            Cesium.Ellipsoid.WGS84.cartographicToCartesian(CartographicPos, CartesianPos);
        
            positions[3*p+0] = CartesianPos.x;
            positions[3*p+1] = CartesianPos.y;
            positions[3*p+2] = CartesianPos.z;
            
            
            //set the normal
            Cesium.Cartographic.fromRadians(region[2], region[1], 0, CartographicPos);
            Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(CartographicPos, CartesianPos);

            normals[3 * p + 0] = CartesianPos.x;
            normals[3 * p + 1] = CartesianPos.y;
            normals[3 * p + 2] = CartesianPos.z;
            p++;
        }
        
        //north east
        //set position
        Cesium.Cartographic.fromRadians(region[2], region[3], 0, CartographicPos);
        Cesium.Ellipsoid.WGS84.cartographicToCartesian(CartographicPos, CartesianPos);

        positions[3*p+0] = CartesianPos.x;
        positions[3*p+1] = CartesianPos.y;
        positions[3*p+2] = CartesianPos.z;


        //set the normal
        Cesium.Cartographic.fromRadians(region[2], region[3], 0, CartographicPos);
        Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(CartographicPos, CartesianPos);

        normals[3 * p + 0] = CartesianPos.x;
        normals[3 * p + 1] = CartesianPos.y;
        normals[3 * p + 2] = CartesianPos.z;
        p++;
        
        
        if(!northPole)
        {
            //north west
            //set position
            Cesium.Cartographic.fromRadians(region[0], region[3], 0, CartographicPos);
            Cesium.Ellipsoid.WGS84.cartographicToCartesian(CartographicPos, CartesianPos);
        
            positions[3*p+0] = CartesianPos.x;
            positions[3*p+1] = CartesianPos.y;
            positions[3*p+2] = CartesianPos.z;
            
            
            //set the normal
            Cesium.Cartographic.fromRadians(region[0], region[3], 0, CartographicPos);
            Cesium.Ellipsoid.WGS84.geodeticSurfaceNormalCartographic(CartographicPos, CartesianPos);

            normals[3 * p + 0] = CartesianPos.x;
            normals[3 * p + 1] = CartesianPos.y;
            normals[3 * p + 2] = CartesianPos.z;
            p++;
        }
        
    }

//        corner = 0;
//        var sw = new Float32Array(positions.buffer, componentBytes * (corner++), 3);
//        if (!southPole) var se = new Float32Array(positions.buffer, componentBytes * (corner++), 3);
//        var ne = new Float32Array(positions.buffer, componentBytes * (corner++), 3);
//        if (!northPole) var nw = new Float32Array(positions.buffer, componentBytes * (corner++), 3);
//
//        setPosition(region[0], region[1], region[0], region[1], 0, sw); // south west
//        if (!southPole) setPosition(region[0], region[1], region[2], region[1], 0, se); // south east
//        setPosition(region[0], region[1], region[2], region[3], 0, ne); // north east
//        if (!northPole) setPosition(region[0], region[1], region[0], region[3], 0, nw); // north west
//
//        corner = 0;
//        setNormal(region[0], region[1], 0, new Float32Array(normals.buffer, componentBytes * (corner++), 3));
//        if (!southPole) setNormal(region[2], region[1], 0, new Float32Array(normals.buffer, componentBytes * (corner++), 3));
//        setNormal(region[2], region[3], 0, new Float32Array(normals.buffer, componentBytes * (corner++), 3));
//        if (!northPole) setNormal(region[0], region[3], 0, new Float32Array(normals.buffer, componentBytes * (corner++), 3));
//
//    //FURTHURE SUBDIVISION OF THE TILE
//    //position array -> 3 * [sw, (!southpole)se, ne, (!northpole)nw]
//    // west, south, east, north, bottom, top <- region


    console.log("size of normals: " + normals.length);
    console.log("size of positions: " + positions.length);
    console.log("no of indices: " + indexCount);
    console.log("nor of vertex: " + vertexCount);

    //SETTING THE MIN AND MAX BOUNDS
    var minPosition = [positions[0], positions[1], positions[2]];
    var maxPosition = [positions[0], positions[1], positions[2]];

    let padding = 0;
    for (let i = 0; i < vertexCount; ++i) {
        minPosition[0] = Math.min(minPosition[0], positions[3 * i + 0] - padding);
        minPosition[1] = Math.min(minPosition[1], positions[3 * i + 1] - padding);
        minPosition[2] = Math.min(minPosition[2], positions[3 * i + 2] - padding);
        maxPosition[0] = Math.max(maxPosition[0], positions[3 * i + 0] + padding);
        maxPosition[1] = Math.max(maxPosition[1], positions[3 * i + 1] + padding);
        maxPosition[2] = Math.max(maxPosition[2], positions[3 * i + 2] + padding);
    }

    
//    indices.set(pole ? [0, 1, 2] : ind);
    //[0, 4, 6, 0, 6, 5, 4, 1, 7, 4, 7, 6, 6, 7, 2, 6, 2, 8, 5, 6, 8, 5, 8, 3]); //[0, 1, 2, 0, 2, 3]);
    
    if(pole)
        indices.set([0,1,2]);
    else
    {
        var ind = [];
        //setup indices for the subdivided tile mesh
        var j = 0;
        for (var i = 0; i < 72; i++) {
            if ((i + 1) % 9 == 0)
                continue;

            ind[6 * j + 0] = i;
            ind[6 * j + 1] = i + 1;
            ind[6 * j + 2] = i + 10;
            ind[6 * j + 3] = i;
            ind[6 * j + 4] = i + 10;
            ind[6 * j + 5] = i + 9;

            j++;
        }
        
        for(var i = 0 ; i < ind.length; i++)
        {
            indices[i] = ind[i];
        }
   
        console.log("indices length: " + indices.length);
    }

    //END PROCEDURAL TERRAIN STUFF

    var buffer = Buffer.concat([
    Buffer.from(indices.buffer),
    Buffer.from(normals.buffer),
    Buffer.from(positions.buffer)
  ], indices.byteLength + normals.byteLength + positions.byteLength);

    // https://github.com/KhronosGroup/glTF/tree/master/specification/1.0/schema
    var gltf = {
        accessors: {
            accessor_ind: {
                bufferView: "bufferView_ind",
                byteOffset: 0,
                byteStride: 0,
                componentType: 5123, // UNSIGNED_SHORT
                count: indexCount,
                type: "SCALAR"
            },
            accessor_nor: {
                bufferView: "bufferViews_attr",
                byteOffset: 0,
                byteStride: 0,
                componentType: 5126, // FLOAT
                count: vertexCount,
                max: [1, 1, 1],
                min: [-1, -1, -1],
                type: "VEC3"
            },
            accessor_pos: {
                bufferView: "bufferViews_attr",
                byteOffset: componentBytes * vertexCount,
                byteStride: 0,
                componentType: 5126, // FLOAT
                count: vertexCount,
                max: maxPosition,
                min: minPosition,
                type: "VEC3"
            }
        },
        asset: {
            premultipliedAlpha: true,
            profile: {
                api: "WebGL",
                version: "1.0.2"
            },
            version: "1.0"
        },
        bufferViews: {
            bufferView_ind: {
                buffer: "Terrain",
                byteLength: indices.byteLength,
                byteOffset: 0,
                target: 34963 // ELEMENT_ARRAY_BUFFER 
            },
            bufferViews_attr: {
                buffer: "Terrain",
                byteLength: normals.byteLength + positions.byteLength,
                byteOffset: indices.byteLength,
                target: 34962 // ARRAY_BUFFER
            }
        },
        buffers: {
            Terrain: {
                byteLength: buffer.byteLength,
                type: "arraybuffer",
                uri: `data:application/octet-stream;base64,${buffer.toString('base64')}`
            }
        },
        materials: {
            material_terrain: {
                name: "MaterialTerrain",
                technique: "technique_terrain",
                values: {
                    diffuse: [0.8, 0.8, 0.8, 1]
                }
            }
        },
        meshes: {
            mesh_terrain: {
                name: "Terrain",
                primitives: [
                    {
                        attributes: {
                            NORMAL: "accessor_nor",
                            POSITION: "accessor_pos",
                        },
                        indices: "accessor_ind",
                        material: "material_terrain",
                        mode: 4 // triangles
          }
        ]
            }
        },
        nodes: {
            node_terrain: {
                children: [],
                matrix: [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1],
                meshes: ["mesh_terrain"],
                name: "Terrain"
            }
        },
        programs: {
            program_terrain: {
                attributes: [
          "a_normal",
          "a_position"
        ],
                fragmentShader: 'terrainFS',
                vertexShader: 'terrainVS'
            }
        },
        scene: "defaultScene",
        scenes: {
            defaultScene: {
                nodes: [
          "node_terrain"
        ]
            }
        },
        shaders: {
            terrainFS: {
                type: 35632,
                uri: "shaders/terrainFS.glsl"
            },
            terrainVS: {
                type: 35633,
                uri: "shaders/terrainVS.glsl"
            }
        },
        techniques: {
            technique_terrain: {
                attributes: {
                    a_normal: "normal",
                    a_position: "position"
                },
                parameters: {
                    "diffuse": {
                        "type": 35666
                    },
                    "modelViewMatrix": {
                        "semantic": "MODELVIEW",
                        "type": 35676
                    },
                    "normal": {
                        "semantic": "NORMAL",
                        "type": 35665
                    },
                    "normalMatrix": {
                        "semantic": "MODELVIEWINVERSETRANSPOSE",
                        "type": 35675
                    },
                    "position": {
                        "semantic": "POSITION",
                        "type": 35665
                    },
                    "projectionMatrix": {
                        "semantic": "PROJECTION",
                        "type": 35676
                    },
                },
                program: "program_terrain",
                states: {
                    enable: [
            2929, // DEPTH_TEST
            2884 // CULL_FACE
          ]
                },
                uniforms: {
                    u_diffuse: "diffuse",
                    u_modelViewMatrix: "modelViewMatrix",
                    u_normalMatrix: "normalMatrix",
                    u_projectionMatrix: "projectionMatrix",
                }
            }
        }
    };

    //  addCesiumRTC(gltf, {
    //    longitude: region[0],
    //    latitude: region[1],
    //    height: 0
    //  });

    const pipelineOptions = {
        basePath: __dirname,
        optimizeForCesium: true
    };

    return processJSON(gltf, pipelineOptions)
        .then(function (optimizedGltf) {
            var gltfWithExtras = addPipelineExtras(optimizedGltf);
            return loadGltfUris(gltfWithExtras);
        })
        .then(function (pipelineGltf) {
            var binaryGltf = getBinaryGltf(pipelineGltf, true, false);
            var glbBuffer = binaryGltf.glb;
            var b3dmBuffer = glbToB3dm(glbBuffer);
            return b3dmBuffer;
        });
}

module.exports = TerrainProvider;

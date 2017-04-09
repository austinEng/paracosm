
'use strict';

function TreeProvider(options) {
  this.generationDepth = options.generationDepth || 4;
  this.rootError = options.rootError || 100000;
  this.errorFactor = options.errorFactor || 0.5;
  this.worldRadius = options.worldRadius || 100000;
  this.maxHeight = options.maxHeight || 1000;
  this.rootRegions = [
    // west, south, east, north, bottom, top
    [ -Math.PI, -Math.PI / 2, 0,           Math.PI / 2, 0, this.maxHeight ],
    [ 0,            -Math.PI / 2, Math.PI, Math.PI / 2, 0, this.maxHeight ]
  ];
}

function getDepth(index) {
  var depth = 0;
  while (index > 0) {
    ++depth;
    index = Math.ceil(index / 4) - 1;
  }
  return depth;
}

TreeProvider.prototype.getRoot = function() {
  return {
    boundingVolume: {
      sphere: [ 0, 0, 0, this.worldRadius ],
    },
    // geometricError: this.rootError,
    refine: 'replace',
    children: [ 
      this.generateNode(0, 1, 1),
      this.generateNode(0, 2, 1),
      this.generateNode(0, 3, 1),
      this.generateNode(0, 4, 1),

      this.generateNode(1, 1, 1),
      this.generateNode(1, 2, 1),
      this.generateNode(1, 3, 1),
      this.generateNode(1, 4, 1),
    ]
  };
}

TreeProvider.prototype.generateNode = function(hemisphere, index, generationDepth) {
  var depth = getDepth(index);
  var error = this.rootError * Math.pow(this.errorFactor, depth);

  var node = {
    boundingVolume: {
      region: this.generateBoundingRegion(hemisphere, index),
    },
    geometricError: error,
    refine: 'replace',
    content: {
      url: `${hemisphere}_${index}.b3dm`
    },
    children: undefined
  }

  if (generationDepth === this.generationDepth) {
    node.content = {
      url: `${hemisphere}_${index}.json`
    }
  } else {
    node.children = [
      this.generateNode(hemisphere, 4 * index + 1, generationDepth + 1),
      this.generateNode(hemisphere, 4 * index + 2, generationDepth + 1),
      this.generateNode(hemisphere, 4 * index + 3, generationDepth + 1),
      this.generateNode(hemisphere, 4 * index + 4, generationDepth + 1),
    ]
  }

  return node;
}

var modifiers = [
  // south west
  function(region) {
    region[0] = region[0];
    region[1] = region[1];
    region[2] = (region[0] + region[2]) / 2;
    region[3] = (region[1] + region[3]) / 2;
  },
  // south east
  function(region) {
    region[0] = (region[0] + region[2]) / 2;
    region[1] = region[1];
    region[2] = region[2];
    region[3] = (region[1] + region[3]) / 2;
  },
  // north west
  function(region) {
    region[0] = region[0];
    region[1] = (region[1] + region[3]) / 2;
    region[2] = (region[0] + region[2]) / 2;
    region[3] = region[3];
  },
  // north east
  function(region) {
    region[0] = (region[0] + region[2]) / 2;
    region[1] = (region[1] + region[3]) / 2;
    region[2] = region[2];
    region[3] = region[3];
  },
];

var indices = [];

TreeProvider.prototype.generateBoundingRegion = function(hemisphere, index) {
  let region = this.rootRegions[hemisphere].slice();

  indices.length = 0;
  while (index > 0) {
    let next = Math.floor(index / 4);
    let childIndex = (index / 4 - next) * 4;
    indices.push(childIndex);
    index = next;
  }

  for(let i = indices.length - 1; i >= 0; --i) {
    modifiers[indices[i]](region);
  }

  return region;
}
  
module.exports = TreeProvider;
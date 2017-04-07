'use strict';

(function() {
  // Parse options to start server
  const yargs = require('yargs').options({
    port: {
      default: 3000,
      description: 'Port to listen on.'
    },
    public: {
      type: 'boolean',
      description: 'Run a public server that listens on all interfaces.'
    },
    help: {
      alias: 'h',
      type: 'boolean',
      description: 'Show this help'
    }
  });
  const argv = yargs.argv;

  if (argv.help) {
    return yargs.showHelp();
  }

  const express = require('express');
  const compression = require('compression');

  const app = express();
  app.use(compression());
  app.use(function(req, res, next) {
    res.header("Access-Control-Allow-Origin", "*");
    res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    next();
  });
  app.set('json spaces', 2);

  const server = app.listen(argv.port, argv.public ? undefined : 'localhost', function() {
    console.log('Terrain server running at http://%s:%d/', server.address().address, server.address().port);
  });

  const RADIUS = 10000;

  // west, south, east, north, bottom, top
  const WEST_ROOT_REGION = [ -Math.PI / 2, -Math.PI / 2, 0, Math.PI / 2, 0, RADIUS ];
  const EAST_ROOT_REGION = [ 0, -Math.PI / 2, Math.PI / 2, Math.PI / 2, 0, RADIUS ];
  const ROOT_REGIONS = [ WEST_ROOT_REGION, EAST_ROOT_REGION ];
  const ROOT_ERROR = 10000;
  const GENERATION_DEPTH = 4;

  function getDepth(index) {
    var depth = 0;
    while (index > 0) {
      ++depth;
      index = Math.ceil(index / 4) - 1;
    }
    return depth;
  }

  function GenerateNode(hemisphere, index, generationDepth) {
    var depth = getDepth(index);
    var error = ROOT_ERROR * Math.pow(0.5, depth);

    var node = {
      boundingVolume: {
        region: GenerateBoundingRegion(hemisphere, index),
      },
      geometricError: error,
      refine: 'replace',
      content: {
        url: `/tileset/${hemisphere}/tile${index}.b3dm`
      },
      children: undefined
    }

    if (generationDepth === GENERATION_DEPTH) {
      node.content = {
        url: `/tileset/${hemisphere}/tile${index}.json`
      }
    } else {
      node.children = [
        GenerateNode(hemisphere, 4 * index + 1, generationDepth + 1),
        GenerateNode(hemisphere, 4 * index + 2, generationDepth + 1),
        GenerateNode(hemisphere, 4 * index + 3, generationDepth + 1),
        GenerateNode(hemisphere, 4 * index + 4, generationDepth + 1),
      ]
    }

    return node;
  }

  const GenerateBoundingRegion = (function() {
    let indices = [];

    let modifiers = [
      function(region) {
        region[0] = region[0];
        region[1] = region[1];
        region[2] = (region[0] + region[2]) / 2;
        region[3] = (region[1] + region[3]) / 2;
      },
      function(region) {
        region[0] = (region[0] + region[2]) / 2;
        region[1] = region[1];
        region[2] = region[2];
        region[3] = (region[1] + region[3]) / 2;
      },
      function(region) {
        region[0] = region[0];
        region[1] = (region[1] + region[3]) / 2;
        region[2] = (region[0] + region[2]) / 2;
        region[3] = region[3];
      },
      function(region) {
        region[0] = (region[0] + region[2]) / 2;
        region[1] = (region[1] + region[3]) / 2;
        region[2] = region[2];
        region[3] = region[3];
      },
    ];

    return function(hemisphere, index) {
      let region = ROOT_REGIONS[hemisphere].slice();

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
  })();
  

  app.get('/tileset.json', function(req, res) {
    res.json({
      asset: {
        version: '0.0',
      },
      geometricError: ROOT_ERROR,
      root: {
        boundingVolume: {
          sphere: [ 0, 0, 0, RADIUS ],
        },
        geometricError: 0,
        refine: 'add',
        children: [ 
          GenerateNode(0, 0, 0),
          GenerateNode(1, 0, 0)
        ]
      },
    });
  });
  
  app.get('/tileset/:hemisphere/tile(:index).json', function(req, res) {
    const index = req.params.index;
    const hemisphere = req.params.hemisphere;

    var node = GenerateNode(hemisphere, parseInt(index), 0);
    res.json({
      asset: {
        version: '0.0'
      },
      geometricError: node.geometricError,
      root: node
    });
  });

  app.get('/tileset/:hemisphere/tile(:index).b3dm', function(req, res) {
    const index = req.params.index;
    const hemisphere = req.params.hemisphere;

    res.send(`Content for node ${index} on hemisphere ${hemisphere}.`);
  });

})();
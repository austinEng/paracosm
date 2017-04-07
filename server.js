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
  const GENERATION_DEPTH = 1;

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

  function GenerateBoundingRegion(hemisphere, index) {
    let region = ROOT_REGIONS[hemisphere].slice();
    return region;
  }

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

    var node = GenerateNode(hemisphere, index, 0);
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
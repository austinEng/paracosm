'use strict';

const SegfaultHandler = require('segfault-handler');
SegfaultHandler.registerHandler("crash.log");

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
  const path = require('path');
  const logger = require('morgan');

  const app = express();
  app.use(compression());
  app.use(function(req, res, next) {
    res.header("Access-Control-Allow-Origin", "*");
    res.header("Access-Control-Allow-Headers", "Origin, X-Requested-With, Content-Type, Accept");
    next();
  });
  app.use(logger('tiny'));
  app.set('json spaces', 2);

  const server = app.listen(argv.port, argv.public ? undefined : 'localhost', function() {
    console.log('Terrain server running at http://%s:%d/', server.address().address, server.address().port);
  });

  const ProceduralWorld = require('bindings')('ProceduralWorld');
  const terrainGenerator = new ProceduralWorld.TerrainGenerator({
    // worldRadius: 6378137,
    ellipsoid: [6378137.0, 6378137.0, 6356752.3142451793],
    maximumDisplacement: 100000,
    persistence: 0.5,
    generationDepth: 3,
    contentGenerationDepth: 3,
  });

  app.get('/tileset.json', function(req, res) {
    var root = terrainGenerator.getRoot();
    res.json({
      asset: {
        version: '0.0',
        gltfUpAxis: 'Z'
      },
      geometricError: root.geometricError,
      root,
    });
  });
  
  app.get('/(:hemisphere)_(:index).json', function(req, res) {
    const index = req.params.index;
    const hemisphere = req.params.hemisphere;

    var node = terrainGenerator.generateNode({
      hemisphere,
      index: parseInt(index),
      generationDepth: 0
    });

    res.json({
      asset: {
        version: '0.0'
      },
      geometricError: node.geometricError,
      root: node
    });
  });

  app.get('/(:hemisphere)_(:index).b3dm', function(req, res) {
    const index = req.params.index;
    const hemisphere = req.params.hemisphere;

    var terrain = terrainGenerator.generateTerrain({
      hemisphere,
      index: parseInt(index)
    });
    res.send(terrain);
  });

  app.use('/Cesium', express.static(path.join(__dirname, 'cesium/Build/CesiumUnminified')));
  app.use('/', express.static(__dirname));
})();
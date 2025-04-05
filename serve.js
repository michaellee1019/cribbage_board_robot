const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const PORT = 8000;
const baseDir = '.pio/build/controller';

const server = http.createServer((req, res) => {
  exec('say "load"');

  const safePath = path.normalize(decodeURIComponent(req.url)).replace(/^(\.\.[\/\\])+/, '');
  let filePath = path.join(baseDir, safePath);

  fs.stat(filePath, (err, stats) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found');
      return;
    }

    if (stats.isDirectory()) {
      filePath = path.join(filePath, 'index.html');
    }

    fs.readFile(filePath, (err, data) => {
      if (err) {
        res.writeHead(404);
        res.end('Not found');
        return;
      }

      res.writeHead(200);
      res.end(data);
    });
  });
});

server.listen(PORT, () => {
  console.log(`Serving ${baseDir} at http://localhost:${PORT}`);
});

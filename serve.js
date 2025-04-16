const http = require('http');
const fs = require('fs');
const path = require('path');
const { exec } = require('child_process');

const PORT = 8000;
const baseDir = '.pio/build/controller';

let reqs = 0;

let sayexec = (st) => {
  console.log(st);
  exec(st);
};

const server = http.createServer((req, res) => {
  reqs++;
  sayexec(`say "start ${reqs}"`);
  // TODO: this is weirdly broken as a feedback mechanism, but the OTA update works. We're seeing double requests
  // and the 'say stop' happens before the response is sent. Or something.
  res.on('finish', () => {
    // Code to execute after the response is sent
    sayexec(`say "stop ${reqs}"`);
  });

  const safePath = path.normalize(decodeURIComponent(req.url)).replace(/^(\.\.[\/\\])+/, '');
  let filePath = path.join(baseDir, safePath);

  fs.stat(filePath, (err, stats) => {
    if (err) {
      res.writeHead(404);
      res.end('Not found ' + filePath);
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

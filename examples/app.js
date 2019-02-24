var util = require('util');
var app = require('http').createServer(handler);
var io = require('socket.io')(app);
var fs = require('fs');
app.listen(3484);

function handler(req, res) {
  fs.readFile(__dirname + '/index.html',
    function (err, data) {
      if (err) {
        res.writeHead(500);
        return res.end('Error loading index.html');
      }

      res.writeHead(200);
      res.end(data);
    });
}
function ParseJson(jsondata) {
  try {
    return JSON.parse(jsondata);
  } catch (error) {
    return null;
  }
}
io.on('connection', function (socket) {
  console.log("Connected");
  socket.emit('welcome', { message: 'Connected !!!!' });
  socket.on('connection', function (data) {
    console.log(data);
  });
  socket.on('atime', function (data, cb) {
    socket.emit('rtime', { time: new Date().toJSON() }, function (_, data) {
      console.log(data);
    });
    if (cb) {
      console.log("sending callback");
      cb({ time: new Date().toJSON() });
    }
    console.log(data);
  });
  socket.on('JSON', function (data) {
    //	console.log(data);
    var jsonStr = JSON.stringify(data);
    var parsed = ParseJson(jsonStr);
    console.log(parsed);
    console.log(parsed.sensor);
  });
  socket.on('arduino', function (data) {
    io.sockets.emit('arduino', { message: 'R0' });
    console.log(data);
  });
});

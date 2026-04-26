const server = net.createServer((socket) => {
  socket.on('data', (data) => {
    socket.write(data);
    socket.end();
  });
});

server.listen(19876, () => {
  const client = net.connect(19876, '127.0.0.1');
  client.write('hello tcp');
  client.on('data', (data) => {
    console.log(data);
    server.close();
  });
});

fs.readFile('/tmp/mari_does_not_exist_xyz.txt', (err, data) => {
  if (err) console.log('error:', err);
  else console.log(data);
});

fs.writeFile('/tmp/mari_test.txt', 'hello from mari', (err) => {
  if (err) { console.error(err); return; }
  fs.readFile('/tmp/mari_test.txt', (err, data) => {
    if (err) { console.error(err); return; }
    console.log(data);
  });
});

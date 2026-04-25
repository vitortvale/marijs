process.exec('exit 1', (err, stdout, stderr) => {
  console.log(err ? 'failed' : 'ok');
});

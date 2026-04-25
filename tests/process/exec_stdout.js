process.exec('echo hello from exec', (err, stdout) => {
  if (err) { console.error(err); return; }
  console.log(stdout.trim());
});

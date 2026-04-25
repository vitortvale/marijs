console.log("start");
Promise.resolve(1)
  .then(v => { console.log("then1:", v); return v + 1; })
  .then(v => { console.log("then2:", v); return v + 1; })
  .then(v => console.log("then3:", v));
console.log("end");

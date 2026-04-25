async function run() {
  console.log("async start");
  console.log("async end");
}

console.log("sync start");
Promise.resolve(run()).then(()=> console.log('resolved'))
console.log("sync end");

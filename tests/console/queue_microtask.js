console.log("start");
queueMicrotask(() => console.log("microtask 1"));
queueMicrotask(() => console.log("microtask 2"));
console.log("end");

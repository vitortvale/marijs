setTimeout(() => {
  console.log("timer 1");
  Promise.resolve().then(() => console.log("microtask after timer 1"));
}, 20);
setTimeout(() => console.log("timer 2"), 60);
Promise.resolve().then(() => console.log("microtask before timers"));

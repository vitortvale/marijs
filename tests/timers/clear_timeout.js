const id = setTimeout(() => console.log("should not fire"), 50);
clearTimeout(id);
setTimeout(() => console.log("done"), 20);

let n = 0;
const id = setInterval(() => {
  console.log("tick", ++n);
  if (n >= 3) clearInterval(id);
}, 20);

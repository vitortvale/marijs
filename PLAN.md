## MariJS v0.1 — Implementation Plan

**Decisions:** Pure C99 | gcc + Makefile | Linux | QuickJS engine only (no quickjs-libc) | epoll + timerfd | CLI: `mari <file.js>`

### Phase 1 — Skeleton + QuickJS embedding
1. Add QuickJS as git submodule under `deps/quickjs/`
2. Create Makefile — compile QuickJS + `src/*.c`, output `mari` binary
3. Write `src/main.c` — init JSRuntime/JSContext, read file from `argv[1]`, `JS_Eval()`, teardown

### Phase 2 — Console bindings
4. Write `src/bindings/console.c` — register `console.log`/`warn`/`error` via `JS_NewCFunction()`
5. Test: `mari test.js` prints "hello marijs"

### Phase 3 — Event loop core
6. Write `src/loop/loop.c` — `epoll_create1`, main loop with microtask draining via `JS_ExecutePendingJob()`, alive handle tracking, graceful exit when idle
7. Wire `mari_loop_run()` into `main.c` after eval

### Phase 4 — Timers
8. Write `src/loop/timers.c` — timerfd_create/settime, epoll registration, repeat support
9. Write `src/bindings/timers.c` — `setTimeout`/`clearTimeout`/`setInterval`/`clearInterval` JS bindings

### Phase 5 — Promise integration
10. Ensure microtask drain after every epoll dispatch round
11. Verify correct execution order: microtasks before next timer

### Phase 6 — Testing + polish
12. Create `tests/` directory with `.js` test scripts and matching `.expected` output files
13. Add `make test` target — runs each test script with `mari`, diffs output against `.expected`
14. Test cases: console output, setTimeout ordering, setInterval + clearInterval, Promise resolution order, nested timers, error handling
15. Error handling — print uncaught exceptions to stderr, exit code 1
16. Makefile polish — `clean` target, `-Wall -Wextra`, dependency tracking

### Backlog (post v0.1)
- **Phase 7**: Async file I/O — `readFile`/`writeFile` using regular fds + epoll
- **Phase 8**: TCP sockets — `net.createServer`, `net.connect`
- **Phase 9**: ES module loader — custom module resolver + `import`/`export` support
- **Phase 10**: Child processes — `exec`/`spawn` with stdin/stdout/stderr piping

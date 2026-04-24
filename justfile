version := `cat deps/quickjs/VERSION`

qjs_src := "deps/quickjs/quickjs.c deps/quickjs/libregexp.c deps/quickjs/libunicode.c deps/quickjs/cutils.c deps/quickjs/dtoa.c"
src     := "src/main.c"
cflags  := "-Wall -Wextra -I deps/quickjs -D_GNU_SOURCE -DCONFIG_VERSION='\"" + version + "\"'"
ldflags := "-lm -ldl -lpthread"

dev:
    gcc {{cflags}} {{qjs_src}} {{src}} -o mari {{ldflags}}

clean:
    rm -f mari

test: dev
    #!/usr/bin/env bash
    set -e
    passed=0; failed=0
    for f in tests/*.js; do
        [[ "$f" == "tests/assert.js" ]] && continue
        name=$(basename "$f")
        if cat tests/assert.js "$f" | ./mari /dev/stdin 2>&1; then
            echo "PASS $name"
            ((passed++))
        else
            echo "FAIL $name"
            ((failed++))
        fi
    done
    echo "$passed passed, $failed failed"
    [[ $failed -eq 0 ]]

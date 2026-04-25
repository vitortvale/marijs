version := `cat deps/quickjs/VERSION`

qjs_src := "deps/quickjs/quickjs.c deps/quickjs/libregexp.c deps/quickjs/libunicode.c deps/quickjs/cutils.c deps/quickjs/dtoa.c"
src     := "src/main.c src/bindings/console.c"
cflags  := "-Wall -Wextra -I deps/quickjs -D_GNU_SOURCE -DCONFIG_VERSION='\"" + version + "\"'"
ldflags := "-lm -ldl -lpthread"

dev:
    gcc {{cflags}} {{qjs_src}} {{src}} -o mari {{ldflags}}

clean:
    rm -f mari

test: dev
    #!/usr/bin/env bash
    passed=0; failed=0
    for f in tests/**/*.js; do
        name=$(basename "$f" .js)
        expected="$(dirname "$f")/$name.expected"
        [ -f "$expected" ] || continue
        actual=$(./mari "$f" 2>&1)
        if diff -q <(echo "$actual") "$expected" > /dev/null 2>&1; then
            echo "PASS $name"
            ((passed++))
        else
            echo "FAIL $name"
            diff <(echo "$actual") "$expected"
            ((failed++))
        fi
    done
    echo "$passed passed, $failed failed"
    [[ $failed -eq 0 ]]

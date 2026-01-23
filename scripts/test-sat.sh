#!/usr/bin/env bash

set -- $CNF/kcolor/k3-400-950/k3-400-*.cnf
total=$#
count=0

echo "Checking ${total} instances with NapSAT..."
echo

# Function to draw a simple progress bar
progress_bar() {
    local progress=$1
    local total=$2
    local width=40
    local filled=$(( progress * width / total ))
    local empty=$(( width - filled ))
    printf "\r["
    printf "%0.s#" $(seq 1 $filled)
    printf "%0.s-" $(seq 1 $empty)
    printf "] %3d/%d" "$progress" "$total"
}

for file; do
    count=$((count + 1))
    progress_bar "$count" "$total"

    output=$(../cadical/build/cadical "$file" 2>/dev/null)
    if echo "$output" | grep -q "s UNSATISFIABLE"; then
        echo "\n❌ UNSAT: $file"
        rm -f "$file"
    fi
done

echo -e "\n\n✅ Done!"

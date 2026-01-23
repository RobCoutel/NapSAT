#!/bin/sh

total=1000
progress_bar() {
    progress=$1
    total=$2
    width=40
    filled=$(( progress * width / total ))
    empty=$(( width - filled ))
    printf "\r["
    i=1; while [ $i -le $filled ]; do printf "#"; i=$((i+1)); done
    i=1; while [ $i -le $empty ]; do printf "-"; i=$((i+1)); done
    printf "] %3d/%d" "$progress" "$total"
}

dir="$CNF/kcolor/k3-400-950-unsat"
mkdir -p "$dir"

i=1
for i in $(seq 1 $total); do
    while true; do
        fname=$(printf "%s/k3-400-%04d.cnf" "$dir" "$i")
        cnfgen kcolor 3 gnm 400 950 > "$fname"
        output=$(../cadical/build/cadical "$fname" 2>/dev/null)
        if echo "$output" | grep -q "s SATISFIABLE"; then
            echo "\n❌ SAT: $fname"
            rm -f "$fname"
        else
            break
        fi
    done
    progress_bar "$i" "$total"
done
echo

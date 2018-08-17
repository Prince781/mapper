#!/bin/bash

if (( $# < 2 )); then
    echo "Usage: $0 scheduler-name num-apps-in-workload [num-runs]"
    exit 1
fi

results_dir=results
scheduler_name=$1
num_apps=$2
num_runs=${3:-3}
regex="^"
anti_regex="^\$"    # don't filter out anything

workloads=()

if [ ! -d $results_dir/$num_apps/$scheduler_name ]; then
    echo "ERROR: $results_dir/$num_apps/$scheduler_name doesn't exist"
    exit 1
fi

for i in $(seq 1 $num_apps); do
    if (( $i == 1 )); then
        regex="^([[:alnum:]]+)"
    else
        regex="$regex-([[:alnum:]]+)"
    fi
done

if [[ $scheduler_name == "linux" ]]; then
    regex="$regex-Linux"
else
    anti_regex="Linux"
fi

regex="$regex.txt\$"

while read fname; do
    if [ ! -e $results_dir/$num_apps/$scheduler_name/$fname.csv ]; then
        echo "-f workloads/$fname";
    fi
done < <(ls workloads/ | grep -E $regex | grep -v $anti_regex) | xargs -t ./jobtest -n $num_runs

echo "Done. After checking the CSVs, place them in $results_dir/$num_apps/$scheduler_name/"

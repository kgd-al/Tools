#!/bin/bash

if [ $# -ne 3 ]
then
  echo "Usage: $0 <outfile> <name> <build-type>"
  exit 10
fi

name=$(basename -s .git `git config --get remote.origin.url`)
buildDate=$(date)
commitHash=$(git describe --dirty --tags --always)
commitMessage=$(git log --format='%B' -n 1 $commitHasH)
commitDate="$(git log --format='%cD' -n 1 $commitHasH) ($(git log --format='%cr' -n 1 $commitHasH))"

out=$1
echo "commitHash: $commitHash" > "$out"
echo "commitMsg: $commitMessage" >> "$out"
echo "commitDate: $commitDate" >> "$out"
[ "$2" = "$name" ] && echo "buildDate: $buildDate" >> "$out"
[ "$3" = "-" ] || echo "buildType: $3" >> "$out"

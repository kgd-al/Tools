#!/bin/bash

if [ $# -ne 1 -o ! -d "$1" ]
then
  echo "Usage: $0 <folder>"
  echo "       Parse all files in the provided folder to generate a dot graph of include dependencies"
  exit 1
fi

dotfile="$1/graph.dot"
echo > $dotfile
echo "digraph {" >> $dotfile
for f in $(find "$1" -type f); do
  bf=$(basename $f)
  for match in $(sed -n "s|#include .\(.*\).|\1|p" $f)
  do
    match=$(basename $match)
    echo "  \"$bf\" -> \"$match\"" >> $dotfile
  done
done
echo "}" >> $dotfile

svgfile=${dotfile%.dot}.svg
dot -Tsvg $dotfile -o $svgfile \
  && cat $dotfile \
  && rm -v $dotfile \
  && echo "Generated $svgfile"
  && xdg-open $svgfile

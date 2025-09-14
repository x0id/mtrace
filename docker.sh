#!/bin/bash

image=mtrace
name=mtrace

while getopts "b" opt; do
  case $opt in
    b)
      docker build . -t $image
      exit $?
      ;;
    *)
      echo "Usage: $0 [-b] [<command to run inside container>]"
      exit 1
      ;;
  esac
done

shift $((OPTIND-1))

opts[k++]="-v $(pwd):/app"
opts[k++]="-w /app"
opts[k++]="--name $name"
opts[k++]="--hostname $name"

if (( $# > 0 )); then
  docker run --rm -it ${opts[@]} $image /bin/bash -lc "$*"
else
  docker run --rm -it ${opts[@]} $image /bin/bash -l
fi

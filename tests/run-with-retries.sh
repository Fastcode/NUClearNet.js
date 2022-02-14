#!/usr/bin/env bash

function fail {
  echo $1 >&2
  exit 1
}

function retryOnFail {
  local n=1
  local max=5
  while true; do
    "$@" && break || {
      if [[ $n -lt $max ]]; then
        ((n++))
        echo ""
        echo ""
        echo "Test failed. Attempt $n/$max:"
        sleep 1;
      else
        echo ""
        echo ""
        fail "Test failed after $n attempts."
        exit 1;
      fi
    }
  done
}

retryOnFail npm test

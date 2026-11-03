#!/bin/bash
case "$1" in
  ''|*[!0-9]*) exit 2 ;;
esac
exec espeak -v en-sc --stdin

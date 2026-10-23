#!/bin/sh
ulimit -s 1024
exec "$@"

#!/bin/bash


./cdn -p 8080 &

./tsd -h 0.0.0.0 -p 8081 -c 8080 -i 1 -t master &
./tsd -h 0.0.0.0 -p 8082 -c 8080 -i 2 -t master &
./tsd -h 0.0.0.0 -p 8083 -c 8080 -i 3 -t master &

./tsd -h 0.0.0.0 -p 8091 -c 8080 -i 1 -t slave &
./tsd -h 0.0.0.0 -p 8092 -c 8080 -i 2 -t slave &
./tsd -h 0.0.0.0 -p 8093 -c 8080 -i 3 -t slave &

./fsn -h 0.0.0.0 -p 9001 -c 8080 -i 1 &
./fsn -h 0.0.0.0 -p 9002 -c 8080 -i 2 &
./fsn -h 0.0.0.0 -p 9003 -c 8080 -i 3 &
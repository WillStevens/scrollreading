#!/bin/sh
for N in {0..9}; do
./simpaper10 nm 3 500 100
cp d:/pipelineOutputPHerc1447/annealStats.csv d:/pipelineOutputPHerc1447/annealStats_${N}.csv
done

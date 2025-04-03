#!/usr/bin/env bash

# Name: runall.sh
# Purpose: Test various branch-prediction modes on six traces,
#          labeling each run's mode in the output file.

# 1) Clean and build
make clean
make

# 2) Start from scratch on results
echo "Branch Predictor Results" > results.txt
echo "========================" >> results.txt

# 3) For each of the 6 traces, run:
#    static, gshare, tournament, custom
for trace in int_1 int_2 fp_1 fp_2 mm_1 mm_2
do
  echo "${trace}:" >> results.txt

  # 3a) STATIC
  echo "  [STATIC]" >> results.txt
  bunzip2 -c ../traces/${trace}.bz2 | ./predictor --static >> results.txt

  # 3b) GSHARE
  echo "  [GSHARE:13]" >> results.txt
  bunzip2 -c ../traces/${trace}.bz2 | ./predictor --gshare:13 >> results.txt

  # 3c) TOURNAMENT
  echo "  [TOURNAMENT:9:10:10]" >> results.txt
  bunzip2 -c ../traces/${trace}.bz2 | ./predictor --tournament:9:10:10 >> results.txt

  # 3d) CUSTOM
  echo "  [CUSTOM]" >> results.txt
  bunzip2 -c ../traces/${trace}.bz2 | ./predictor --custom >> results.txt

  echo "======" >> results.txt
done

# Done!
echo "All results are in results.txt"

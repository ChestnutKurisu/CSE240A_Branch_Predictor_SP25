#!/usr/bin/env bash
#
# run_extended_experiments.sh
# Sweep across various parameters to gather broader results
#

# 1) Build
make clean
make

# 2) Output file
EXTENDED_RESULTS="extended_results.txt"
rm -f $EXTENDED_RESULTS
echo "Extended Branch Predictor Experiments" > $EXTENDED_RESULTS
echo "======================================" >> $EXTENDED_RESULTS

# 3) Parameter sweeps

# Sweep gshare from history=8..16
for ghist_bits in 8 9 10 11 12 13 14 15 16
do
  echo "GSHARE - ghistoryBits=${ghist_bits}" >> $EXTENDED_RESULTS
  for trace in int_1 int_2 fp_1 fp_2 mm_1 mm_2
  do
    echo "Trace: ${trace}" >> $EXTENDED_RESULTS
    bunzip2 -c ../traces/${trace}.bz2 | ./predictor --gshare:${ghist_bits} >> $EXTENDED_RESULTS
    echo "------" >> $EXTENDED_RESULTS
  done
  echo "======" >> $EXTENDED_RESULTS
done

# Sweep some Tournament combos
for GH in 9 10 11 12 13 14; do
 for LH in 9 10 11 12; do
   for PC in 9 10 11 12; do
     echo "TOURNAMENT - gh=${GH}, lh=${LH}, pc=${PC}" >> $EXTENDED_RESULTS
     for trace in int_1 int_2 fp_1 fp_2 mm_1 mm_2
     do
       echo "Trace: ${trace}" >> $EXTENDED_RESULTS
       bunzip2 -c ../traces/${trace}.bz2 | ./predictor --tournament:${GH}:${LH}:${PC} >> $EXTENDED_RESULTS
       echo "------" >> $EXTENDED_RESULTS
     done
     echo "======" >> $EXTENDED_RESULTS
   done
 done
done

# Try custom predictor with small modifications if relevant
echo "CUSTOM TAGE tests..." >> $EXTENDED_RESULTS
for trace in int_1 int_2 fp_1 fp_2 mm_1 mm_2
do
 echo "Trace: ${trace}" >> $EXTENDED_RESULTS
 bunzip2 -c ../traces/${trace}.bz2 | ./predictor --custom >> $EXTENDED_RESULTS
 echo "------" >> $EXTENDED_RESULTS
done
echo "======" >> $EXTENDED_RESULTS

echo "All extended results saved to ${EXTENDED_RESULTS}."

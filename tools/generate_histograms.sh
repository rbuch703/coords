#!/bin/sh

#reads a key-value dump, and outputs key-value and key histograms

sort $1 | uniq -c| sort -nr > kv_histogram.csv

python key_freq.py kv_histogram.csv > key_histogram.csv

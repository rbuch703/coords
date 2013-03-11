#! /bin/sh

lcov --capture --directory . --output-file coverage.info
mkdir html
genhtml coverage.info --output-directory html

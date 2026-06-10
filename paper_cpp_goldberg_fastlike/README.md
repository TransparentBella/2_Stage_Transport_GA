# Goldberg-style C++ GA Experiment

This directory is a separate experiment for replacing the C++ GA flow evaluator with a Goldberg-style cost-scaling minimum-cost-flow backend.

The existing `GA _for_SCFLP/paper_cpp_ga` implementation is left untouched and may continue running.

## Goal

- Keep the GA method and parameters aligned with Fernandes et al. (2014).
- Replace the fixed-facility subproblem solver with a cost-scaling minimum-cost-flow implementation in the Goldberg family.
- Run smoke tests before using it for formal experiments.

## Status

This is an experimental branch. The first implementation step is to build and verify the cost-scaling solver against the already working successive-shortest-path C++ version on small GA smoke tests.

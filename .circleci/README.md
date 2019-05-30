## CircleCI build scripts

The `.circleci` directory contains scripts for each build step in each build stage.
Currently the travis build defines to stages `build` and `test/lint`.
Every script in here is named and numbered according to which stage and lifecycle
step it belongs to.

Also a release cycles is existing to support the idea of a continous delivery.

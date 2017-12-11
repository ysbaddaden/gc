# Garbarge Collector

A garbage collector for the Crystal programming language. Currently written in
C, may eventually be ported to pure Crystal (or kept as an external library).

The garbage collector will be based on Immix, a garbage collector introduced by
Stephen M. Blackburn and Kathryn S. McKinley in the
[Immix: A Mark-Region Garbage Collector with Space Efficiency, Fast Collection,
and Mutator Performance](http://www.cs.utexas.edu/users/speedway/DaCapo/papers/immix-pldi-2008.pdf)
paper published in 2008.

Only the basic ideas from Immix shall be implemented at first, one at a time,
with the goal to implement all Immix optimizations (precise GC, compaction, ...)
in the long term.


## WARNING

It's a WORK IN PROGRESS. Only a linked-list based object space as been
implemented (which will become the large objet space), and only the allocator
has been implemented, too, which means there are no collector, yet.

This little should already be usable, thought not any better than `gc/none`,
until we have at least a working collector for this object space.


## Tests

First install the ([greatest](https://github.com/silentbicycle/greatest/)
dependency using `make install` then run `make test` to run the test suites.

Please note that the test suites are merely scaffold tests for TDD data
structures for the time being.


## Credits

- Author: Julien Portalier [ @ysbaddaden ]

Design is based on the ImmixGC collector from the ScalaNative project, and the
Immix paper from 2008 (see above for link).

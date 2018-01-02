# Garbarge Collector

A garbage collector for the Crystal programming language. Currently written in
C, may eventually be ported to pure Crystal (or kept as an external library).

The garbage collector is based on Immix, a garbage collector introduced by
Stephen M. Blackburn and Kathryn S. McKinley in the
[Immix: A Mark-Region Garbage Collector with Space Efficiency, Fast Collection,
and Mutator Performance](http://www.cs.utexas.edu/users/speedway/DaCapo/papers/immix-pldi-2008.pdf)
paper published in 2008.

Only the basic ideas from Immix shall be implemented at first, one at a time,
with the goal to implement all Immix optimizations (precise GC, compaction, ...)
in the long term.


## Status

It's a WORK IN PROGRESS.

The small object space is following the Immix memory layout, minus compaction
(opportunistic evacuation) which requires a fully precise GC capable to
perfectly identify references in both allocations and stacks.

The large objects space relies on a mere linked list of splittable chunks. There
is room for improvement, here. Mostly in indexing of free/allocated chunks for
quicker allocations and quicker marking.

Allocators and a collector have been implemented. Thought they appear to be
correct, they're only efficient during certain workloads. Some workloads, like
the Crystal compiler, that rapidly allocate many objects and requires the HEAP
to grow quickly may be very slow (the GC will collect too often), or keeping a
deeply nested object tree will cause a stack overflow. We miss mechanisms to
prefer growing the HEAP over collecting again and again quickly for little gains
(slooooow) and dispatching marking of nested objects instead of recursively
marking objects and exhausting the stack (segfaults).

You've been warned!


## Tests

First install the ([greatest](https://github.com/silentbicycle/greatest/)
dependency using `make install` then run `make test` to run the test suites.

Please note that the test suites are merely scaffold tests for TDD of data
structures.


## Credits

- Author: Julien Portalier [ @ysbaddaden ]

Design is based on the ImmixGC collector from the ScalaNative project, and the
Immix paper from 2008 (see above for link).

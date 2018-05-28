# Garbarge Collector

A garbage collector for the Crystal programming language.

Currently written in C. It may eventually be ported to Crystal, or kept as an
external library for generic usages —with strong ties to Crystal.

The garbage collector is based on Immix, a garbage collector introduced by
Stephen M. Blackburn and Kathryn S. McKinley in the
[Immix: A Mark-Region Garbage Collector with Space Efficiency, Fast Collection, and Mutator Performance](http://www.cs.utexas.edu/users/speedway/DaCapo/papers/immix-pldi-2008.pdf)
paper published in 2008.

Only the basic ideas from Immix are implemented. The goal is to implement most
optimizations, for example compaction and precise marking in the long term.


## Status

This is a WORK IN PROGRESS but seems to be CORRECT —i.e. no segfaults!

The small object space follows the Immix memory layout, minus compaction
(opportunistic evacuation) that would require a fully precise GC capable to
perfectly identify references in both allocations and stacks.

The large object space relies on a mere linked list of splittable chunks. There
is room for improvement, mostly in indexing of free/allocated chunks for quicker
allocations and marking.

The GC appears to be correct, and capable to handle different workloads with
good performance. For example a highly concurrent HTTP server, or the Crystal
compiler itself.

Please try it out and report issues —e.g. slower cases, high memory usages.
Don't forget to add reduced samples that reproduce the issue.


## Usage

I recommend using Crystal 0.24.2 or later.

Add the `immix` dependency to your `shard.yml`, then run `shards install` or
`shards update`. This will install the dependency and compile the `immix.a`
library automatically.

```yaml
dependencies:
  immix:
    github: ysbaddaden/gc
    branch: master
```

Require `immix` somewhere in your source code, then build your program with the
`-Dgc_none` flag. This combinaison will use the dummy GC provided with Crystal
that will be overwritten with this shard's GC.

If you want to conduct tests, you may conditionally require immix GC based on
the presence of the `gc_none` flag. For example:

```crystal
{% if flag?(:gc_none) %}
  require "immix"
{% end %}
```


### Benchmarks

Installing the `immix` shard will compile `immix.a` with assertions turned on,
to help debugging and issue reporting. To fully evaluate the performance, you
should recompile the library with `-DNDEBUG` to disable assertions. For example:

```console
$ cd lib/immix
$ make -B CUSTOM=-DNDEBUG
```


## Tests

First install the ([greatest](https://github.com/silentbicycle/greatest/)
dependency using `make setup` then run `make test` to run the test suite.

Note that the test suites are merely scaffold tests for TDD of data structures.


## Credits

- Author: Julien Portalier [ @ysbaddaden ]

Design is based on the ImmixGC collector from the ScalaNative project, and the
Immix paper from 2008 (see above for link).

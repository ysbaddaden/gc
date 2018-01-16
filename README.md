# Garbarge Collector

A garbage collector for the Crystal programming language. Currently written in
C. It may eventually be ported to Crystal, or kept as an external library for
generic usages —with strong ties to Crystal.

The garbage collector is based on Immix, a garbage collector introduced by
Stephen M. Blackburn and Kathryn S. McKinley in the
[Immix: A Mark-Region Garbage Collector with Space Efficiency, Fast Collection,
and Mutator Performance](http://www.cs.utexas.edu/users/speedway/DaCapo/papers/immix-pldi-2008.pdf)
paper published in 2008.

Only the basic ideas from Immix will be implemented at first, with the goal to
eventually implement most Immix optimizations (compaction) along with precise
marking in the long term.


## Status

It's a WORK IN PROGRESS that looks like working.

The small object space is following the Immix memory layout, minus compaction
(opportunistic evacuation) which requires a fully precise GC capable to
perfectly identify references in both allocations and stacks.

The large objects space relies on a mere linked list of splittable chunks. There
is room for improvement, here. Mostly in indexing of free/allocated chunks for
quicker allocations and quicker marking.

Allocators and a collector have been implemented. Thought they appear to be
correct, they're only efficient during certain workloads. Some workloads, like
the Crystal compiler, that rapidly allocate many objects and require the HEAP to
grow quickly may be very slow (the GC will collect too often). Keeping a deeply
nested object tree may also cause a stack overflow.

The allocator misses a mechanism to prefer growing the HEAP over collecting
again and again quickly for small gains if any (very slow). The collector should
dispatch marking of nested objects instead of recursively marking objects,
exhausting the stack, and causing segfaults.

Last but not least, finalizers aren't called yet, so you'll probably leak
memory (not controlled by GC) and more.

You've been warned!


## Usage

I recommend using Crystal strictly greater than 0.24.1 (i.e. master branch).

Add the `immix` dependency to your `shard.yml`, then run `shards install` or
`shards update`. This will install the dependency and compile the `immix.a`
library automatically.

```yaml
dependencies:
  immix:
    github: ysbaddaden/gc
    branch: master
```

Require `immix` somewhere in your crystal source code, then compile it with the
`-Dgc_none` flag. This combination will use the dummy GC provided with crystal
which will be overwriten with this shard's GC.

If you want to conduct tests, you may conditionally require immix GC based on
the presence of the `gc_none` flag, for example:

```crystal
{% if flag?(:gc_none) %}
  require "immix"
{% end %}
```


## Tests

First install the ([greatest](https://github.com/silentbicycle/greatest/)
dependency using `make setup` then run `make test` to run the test suites.

Please note that the test suites are merely scaffold tests for TDD of data
structures.


## Credits

- Author: Julien Portalier [ @ysbaddaden ]

Design is based on the ImmixGC collector from the ScalaNative project, and the
Immix paper from 2008 (see above for link).

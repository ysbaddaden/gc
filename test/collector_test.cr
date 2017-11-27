require "minitest/autorun"
require "../src/collector"
require "../src/local_allocator"

module GC
  class CollectorTest < Minitest::Test
    @ga = GlobalAllocator.new(SizeT.new(BLOCK_SIZE) * 2)
    @la = LocalAllocator.new(pointerof(@ga))
    @collector = Collector.new(pointerof(@ga), pointerof(@la))

    # Scenario 1: create small/medium allocations in single block. Some
    # references are lost, some are kept, a reference to a medium object is lost
    # but an inner pointer is kept (so the object must be marked).
    def test_scenario1
      small1 = Pointer(Void).null

      # create 2 small object (second will shadow first)
      2.times do
        small1 = @la.allocate_small(SizeT.new(32))
      end

      # create small object that spans lines
      small2 = @la.allocate_small(SizeT.new(LINE_SIZE - sizeof(Word)))

      # create a medium object (shadowed later) + inner pointer
      medium1 = @la.allocate_small(SizeT.new(LINE_SIZE * 2))
      pointer_to_medium1 = medium1 + (LINE_SIZE * 2) - 32

      # overwrite medium object
      medium1 = @la.allocate_small(SizeT.new(LINE_SIZE * 2))

      # collect memory
      @collector.collect

      # find object for allocations:
      o_small1_orig = (small1 - sizeof(Object) * 2 - 32).as(Object*)
      o_small1 = (small1 - sizeof(Object)).as(Object*)
      o_small2 = (small2 - sizeof(Object)).as(Object*)
      o_medium1_orig = (pointer_to_medium1 - (LINE_SIZE * 2) + 32 - sizeof(Object)).as(Object*)
      o_medium1 = (medium1 - sizeof(Object)).as(Object*)

      # verify objects have been marked (or not):
      refute o_small1_orig.value.marked?, "expected not to mark object (without reference)"
      assert o_small1.value.marked?, "expected to mark live object (that replaced a reference)"
      assert o_small2.value.marked?, "expected to mark object with stack reference"
      assert o_medium1_orig.value.marked?, "expected to mark object with referenced inner pointer (but no direct reference)"
      assert o_medium1.value.marked?, "expected to mark medium-sized object (with reference)"

      block = Block.from(small1)
      assert block.value.marked?
      assert block.value.line_header_at(0).value.marked?
    end

    # Scenario 2: almost fill 1st block, then allocate an object that doesn't
    # fit in remaining, so it's allocated in the 2nd block. Collect twice, the
    # second time after removing all references to first block. Only the 2nd
    # block should be marked.
    def test_scenario2
      # fill first block:
      m1 = Pointer(Void).null
      4.times { m1 = @la.allocate_small(SizeT.new(8128 - sizeof(Object))) }

      # allocate one object to second block:
      m2 = @la.allocate_small(SizeT.new(LINE_SIZE))

      b1 = Block.from(m1)
      b2 = Block.from(m2)
      refute_equal b1, b2

      assert b1.value.line_header_at(0).value.contains_object_header?
      assert b1.value.line_header_at(31).value.contains_object_header?
      assert b1.value.line_header_at(63).value.contains_object_header?
      assert b1.value.line_header_at(95).value.contains_object_header?
      assert b2.value.line_header_at(0).value.contains_object_header?

      # collect memory: should mark both blocks
      @collector.collect
      assert b1.value.marked?, "expected to mark 1st block on first collect"
      assert b2.value.marked?, "expected to mark 2nd block on first collect"

      refute b1.value.line_header_at(0).value.marked?, "expected lost reference to not be marked"
      refute b1.value.line_header_at(31).value.marked?, "expected lost reference to not be marked"
      refute b1.value.line_header_at(63).value.marked?, "expected lost reference to not be marked"
      assert b1.value.line_header_at(95).value.marked?, "expected last referenced object in first block to be marked"
      assert b2.value.line_header_at(0).value.marked?, "expected referenced object in second block to be marked"

      assert b1.value.recyclable?, "expected 1st block to be moved to recycled list"
      assert b2.value.recyclable?, "expected 2nd block to be moved to recycled list"

      # erase reference to object from block1:
      m1 = Pointer(Void).null

      # collect again: should only mark block 2:
      @collector.collect
      refute b1.value.marked?, "expected to not mark 1st block on second collect"
      assert b2.value.marked?, "expected to mark 2nd block on second collect"

      assert b1.value.free?, "expected 1st block to be moved to free list"
      assert b2.value.recyclable?, "expected 2nd block to be moved to recycled list"

      refute b1.value.line_header_at(95).value.marked?
      assert b2.value.line_header_at(0).value.marked?
    end

    # Scenario 3: fill a block with local references. Collect. The previously
    # filled block should be unavailable. It should respect conservative marking
    # when deciding to move blocks to the free, recyclable or unavailable list
    # (since conservative marking skips marking the line an object spans to,
    # then the collector must skip a free line when deciding whether a block has
    # a hole and thus can be recycled or not (thus be unavailable).
    def test_recycles_blocks
      # 1st block (last line is kept empty):
      m1 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m2 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m3 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m4 = @la.allocate_small(SizeT.new(7872 - sizeof(Object)))

      # 2nd block:
      m5 = @la.allocate_small(SizeT.new(512 - sizeof(Object)))
      m5 = @la.allocate_small(SizeT.new(256 - sizeof(Object)))

      b1 = Block.from(m1)
      b2 = Block.from(m5)

      @collector.collect

      assert b1.value.unavailable?, "expected full block to be unavailable"

      assert b2.value.recyclable?, "expected block with some marked lines to be recycled"
      assert_equal 0, b2.value.first_free_line_index

      # erase some references:
      m2 = Pointer(Void).null
      m5 = Pointer(Void).null

      @collector.collect

      assert b1.value.recyclable?, "expected block with hole to be recycled"
      assert_equal 33, b1.value.first_free_line_index

      assert b2.value.free?, "expected empty block to be free"
      assert_equal 0, b2.value.first_free_line_index
    end

    # Scenario 4: after collect, the block/cursor/limit of local allocators must
    # have been resetted, trying to allocate should fetch recyclable blocks (in
    # address order), then free blocks (in address order), then grow memory and
    # allocate in fresh free blocks.
    def test_scenario4
      # 1st block (filled, lost references):
      m0 = Pointer(Void).null
      4.times { m0 = @la.allocate_small(SizeT.new(8128 - sizeof(Object))) }

      # 2nd block (filled, with references):
      m1 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m2 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m3 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))
      m4 = @la.allocate_small(SizeT.new(7872 - sizeof(Object)))

      # 3rd block (heap grow, filled, lost references):
      m5 = Pointer(Void).null
      4.times { m5 = @la.allocate_small(SizeT.new(8128 - sizeof(Object))) }

      # 4th block (heap grow, current allocator cursor):
      m6 = @la.allocate_small(SizeT.new(8128 - sizeof(Object)))

      b1 = Block.from(m0); m0 = Pointer(Void).null
      b2 = Block.from(m1)
      b3 = Block.from(m5)
      b4 = Block.from(m6)

      @collector.collect

      assert b1.value.free?, "expected 1st block to be free"
      assert b2.value.unavailable?, "expected 2nd block to be unavailable"
      assert b3.value.recyclable?, "expected 3rd block to be recyclable"
      assert b4.value.recyclable?, "expected 4th block to be recyclable"

      # exhaust recyclable blocks:
      m7 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      assert_equal b3, Block.from(m7), "expected to allocate into recycled block (exhausting recyclable block)"

      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      assert_equal b4, Block.from(m8), "expected to allocate into recycled block (exhausting another recyclable block)"

      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      assert_equal b1, Block.from(m8), "expected to allocate into free block (exhausted recyclable blocks)"

      # exhaust free blocks:
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))

      # grow memory: allocate in new block
      m8 = @la.allocate_small(SizeT.new(8128) - sizeof(Object))
      refute_equal b1, Block.from(m8), "expected to allocate into new block (exhausted free blocks)"
    end

    # Scenario 5 (conservative marking):
    #
    # The collector only marks the starting line for small objects (smaller than
    # LINE_SIZE), yet small objects may span one line, so when recycling a block,
    # the collector must skip a freeline.
    #
    # Since the collector only marks the starting line of small objects, but
    # small objects may span a line, the allocator must skip a line whenever
    # searching for a hole.
    #
    # Medium objects (larger than LINE_SIZE) must be exactly marked, except for
    # the last line they are allocated to, since the allocator and collector
    # will skip a free line when allocating.
    def test_conservative_marking
      first  = @la.allocate_small(SizeT.new(128))
      second = @la.allocate_small(SizeT.new(224))
      lost1  = @la.allocate_small(SizeT.new(1024) - sizeof(Object)) # first hole
      third  = @la.allocate_small(SizeT.new(128))
      lost2  = @la.allocate_small(SizeT.new(1024) - sizeof(Object)) # second hole
      fourth = @la.allocate_small(SizeT.new(128))
      medium = @la.allocate_small(SizeT.new(1024))

      lost1 = lost2 = Pointer(Void).null

      @collector.collect

      b1 = Block.from(first)
      assert b1.value.line_header_at(0).value.marked?  # first, second
      refute b1.value.line_header_at(1).value.marked?  # second (span)
      refute b1.value.line_header_at(2).value.marked?  # hole
      refute b1.value.line_header_at(3).value.marked?  # hole
      refute b1.value.line_header_at(4).value.marked?  # hole
      assert b1.value.line_header_at(5).value.marked?  # third
      refute b1.value.line_header_at(6).value.marked?  # hole (skipped)
      refute b1.value.line_header_at(7).value.marked?  # hole
      refute b1.value.line_header_at(8).value.marked?  # hole
      refute b1.value.line_header_at(9).value.marked?  # hole
      assert b1.value.line_header_at(10).value.marked? # fourth
      assert b1.value.line_header_at(11).value.marked? # medium
      assert b1.value.line_header_at(12).value.marked? # medium (span)
      assert b1.value.line_header_at(13).value.marked? # medium (span)
      refute b1.value.line_header_at(14).value.marked? # medium (skipped)

      # must be allocated in first hole (found by collector when recycling block):
      fifth = @la.allocate_small(SizeT.new(3 * 248) - sizeof(Object))
      assert_equal b1.value.line_at(2) + sizeof(Object), fifth

      # must be allocated in second hole (found by allocator when searching hole):
      a = @la.allocate_small(SizeT.new(256) - sizeof(Object))
      b = @la.allocate_small(SizeT.new(256) - sizeof(Object))
      c = @la.allocate_small(SizeT.new(256) - sizeof(Object))
      assert_equal b1.value.line_at(7) + sizeof(Object), a
      assert_equal b1.value.line_at(8) + sizeof(Object), b
      assert_equal b1.value.line_at(9) + sizeof(Object), c

      # must be allocated in remaining space in block (found by allocator when searching hole):
      d = @la.allocate_small(SizeT.new(256) - sizeof(Object))
      assert_equal b1.value.line_at(15) + sizeof(Object), d
    end

  end
end

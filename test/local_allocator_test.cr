require "minitest/autorun"
require "../src/local_allocator"

module GC
  class LocalAllocatorTest < Minitest::Test
    WORD_SIZE = SizeT.new(GC::WORD_SIZE)

    @ga = GlobalAllocator.new(SizeT.new(BLOCK_SIZE) * 2)

    def test_allocate_small
      la = LocalAllocator.new(pointerof(@ga))

      # 'small' object (< LINE_SIZE):
      a1 = la.allocate_small(SizeT.new(64))
      o1 = (a1 - sizeof(Object)).as(Object*)
      assert_equal 64 + sizeof(Object), o1.value.size
      assert o1.value.standard?

      # 'small' object may span over two lines:
      a2 = la.allocate_small(SizeT.new(192))
      o2 = (a2 - sizeof(Object)).as(Object*)
      assert_equal 192 + sizeof(Object), o2.value.size
      assert o2.value.standard?

      # objects are bump allocated:
      assert_equal a1 + o1.value.size, a2

      # medium object (< LARGE_OBJECT_SIZE):
      a3 = la.allocate_small(SizeT.new(256))
      o3 = (a3 - sizeof(Object)).as(Object*)
      assert_equal 256 + sizeof(Object), o3.value.size
      assert o3.value.standard?

      # objects are bump allocated:
      assert_equal a2 + o2.value.size, a3
    end

    def test_allocate_small_updates_line_header
      la = LocalAllocator.new(pointerof(@ga))

      # first object on line #0 (no offset):
      a1 = la.allocate_small(SizeT.new(32))
      lh0 = la.@block.value.line_header_at(0)
      assert lh0.value.contains_object_header?
      assert_equal 0, lh0.value.offset

      # small object span to line #1:
      a2 = la.allocate_small(SizeT.new(248))

      # didn't update line #0 (not first object):
      assert lh0.value.contains_object_header?
      assert_equal 0, lh0.value.offset

      # didn't update line #1 (not first object):
      lh1 = la.@block.value.line_header_at(1)
      refute lh1.value.contains_object_header?

      # first object on line #1 (with offset):
      a3 = la.allocate_small(SizeT.new(64))
      assert lh1.value.contains_object_header?

      offset = (32 + 248 + sizeof(Object) * 2) % LINE_SIZE
      assert_equal offset, lh1.value.offset
    end

    def test_allocate_small_zeroes_next_object_size_on_same_line
      # initialize block with garbage
      block = @ga.next_block
      LibC.memset(block, 0xff, BLOCK_SIZE)
      block.value.initialize
      @ga.recyclable(block)

      la = LocalAllocator.new(pointerof(@ga))

      # zeroes next object's size on the same line:
      a1 = la.allocate_small(SizeT.new(16))
      assert_equal 0, (a1 + 16).as(Word*).value

      # won't zero next object's size across line:
      remaining = LINE_SIZE - 16 - sizeof(Object) * 2
      la.allocate_small(SizeT.new(remaining))
      refute_equal 0, (a1 + remaining).as(Word*).value
    end

    def test_allocate_large
      la = LocalAllocator.new(pointerof(@ga))

      remaining = LINE_SIZE * LINE_COUNT - 8192 * 2 - sizeof(Object) * 3

      # allocate large objects (fills large heap)
      a1 = la.allocate_large(SizeT.new(8192))
      a2 = la.allocate_large(SizeT.new(8192))
      a3 = la.allocate_large(SizeT.new(remaining))

      o1 = (a1 - sizeof(Object)).as(Object*)
      o2 = (a2 - sizeof(Object)).as(Object*)
      o3 = (a3 - sizeof(Object)).as(Object*)

      c1 = (a1 - sizeof(Chunk)).as(Chunk*)
      c2 = (a2 - sizeof(Chunk)).as(Chunk*)
      c3 = (a3 - sizeof(Chunk)).as(Chunk*)
      c4 = (a3 + remaining).as(Chunk*)

      # objects are bump allocated
      assert_equal a1 + 8192 + sizeof(Chunk), a2
      assert_equal a2 + 8192 + sizeof(Chunk), a3

      # objects are initialized
      assert_equal 8192 + sizeof(Object), o1.value.size
      assert_equal 8192 + sizeof(Object), o2.value.size
      assert_equal remaining + sizeof(Object), o3.value.size

      assert o1.value.large?
      assert o2.value.large?
      assert o3.value.large?

      refute o1.value.standard?
      refute o2.value.standard?
      refute o3.value.standard?

      # chunks are initialized
      assert c1.value.allocated?
      assert c2.value.allocated?
      assert c3.value.allocated?
      assert c4.value.free?

      assert_equal c2, c1.value.next
      assert_equal c3, c2.value.next
      assert_equal c4, c3.value.next
      assert_equal Pointer(Chunk).null, c4.value.next

      # force large heap to grow
      a4 = la.allocate_large(SizeT.new(8192))
      o4 = (a4 - sizeof(Object)).as(Object*)
      c4 = (a4 - sizeof(Chunk)).as(Chunk*)

      assert_equal a3 + remaining + sizeof(Chunk), a4
      assert c4.value.allocated?

      # find last chunk (free space)
      c5 = (a4 + 8192).as(Chunk*)
      assert c5.value.free?

      assert_equal c4, c3.value.next
      assert_equal c5, c4.value.next
    end
  end
end

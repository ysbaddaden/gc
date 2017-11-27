require "minitest/autorun"
require "../src/global_allocator"

module GC
  class GlobalAllocatorTest < Minitest::Test
    @heap : Pointer(Void)?

    def test_next_block
      object_size = SizeT.new(LINE_SIZE)

      # initial: 0 recyclable, 2 free, 0 unavailable blocks
      ga = GlobalAllocator.new(SizeT.new(BLOCK_SIZE) * 2)
      assert_equal BLOCK_SIZE * 2, ga.heap_size

      # initial: exhaust free list
      b1 = ga.next_block
      assert b1.value.free?

      b2 = ga.next_block
      assert b2.value.free?

      # initial: grow
      b3 = ga.next_block
      assert_equal BLOCK_SIZE * 3, ga.heap_size

      # collect (simulate):
      ga.recyclable(b1)
      ga.recyclable(b2)
      ga.free(b3)
      assert_equal BLOCK_SIZE * 3, ga.heap_size

      # steady state allocation: exhaust recyclable blocks
      assert_equal b1, ga.next_block
      assert b1.value.recyclable?

      assert_equal b2, ga.next_block
      assert b2.value.recyclable?

      # steady state allocation: exhaust free blocks
      assert_equal b3, ga.next_block
      assert b3.value.free?
      assert_equal BLOCK_SIZE * 3, ga.heap_size

      # steady state allocation: grow
      b4 = ga.next_block
      assert b4.value.free?
      assert_equal BLOCK_SIZE * 4, ga.heap_size
    end
  end
end

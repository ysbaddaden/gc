require "minitest/autorun"
require "../src/memory"
require "../src/block_list"

module GC
  class ListTest < Minitest::Test
    @heap : Pointer(Void)?

    def heap
      @heap ||= GC.map_and_align(SizeT.new(BLOCK_SIZE) * 4, SizeT.new(BLOCK_SIZE) * 2)
    end

    def block(n)
      (heap + BLOCK_SIZE * n).as(Block*)
    end

    # FIXME: randomly segfaults in `List#push`!
    def test_push_and_shift
      list = BlockList.new
      assert_equal 0, list.size

      list << block(1)
      assert_equal 1, list.size
      assert_equal Pointer(Block).null, block(1).value.next

      list << block(2)
      list << block(3)
      assert_equal 3, list.size
      assert_equal block(2), block(1).value.next
      assert_equal block(3), block(2).value.next
      assert_equal Pointer(Block).null, block(3).value.next

      assert_equal block(1), list.shift
      assert_equal 2, list.size

      assert_equal block(2), list.shift
      assert_equal 1, list.size

      assert_equal block(3), list.shift
      assert_equal 0, list.size
      assert_equal Pointer(Block).null, list.shift

      list << block(3)
      list << block(1)
      assert_equal 2, list.size

      assert_equal block(1), block(3).value.next
      assert_equal Pointer(Block).null, block(1).value.next

      assert_equal block(3), list.shift
      assert_equal block(1), list.shift
      assert_equal Pointer(Block).null, list.shift
    end

    def test_empty?
      list = BlockList.new
      assert list.empty?

      list << block(1)
      refute list.empty?

      list.shift
      assert list.empty?
    end

    def test_clear
      list = BlockList.new
      list << block(1)
      list << block(2)
      refute list.empty?

      list.clear
      assert list.empty?
    end
  end
end

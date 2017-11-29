require "minitest/autorun"
require "../src/memory"
require "../src/block"
require "../src/linked_list"

module GC
  class LinkedListTest < Minitest::Test
    @heap : Pointer(Void)?

    def heap
      @heap ||= GC.map_and_align(SizeT.new(BLOCK_SIZE) * 6, SizeT.new(BLOCK_SIZE) * 2)
    end

    def block(n)
      (heap + BLOCK_SIZE * n).as(Block*)
    end

    def test_push_and_shift
      list = LinkedList(Block).new
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
      list = LinkedList(Block).new
      assert list.empty?

      list << block(1)
      refute list.empty?

      list.shift
      assert list.empty?
    end

    def test_clear
      list = LinkedList(Block).new
      list << block(1)
      list << block(2)
      refute list.empty?

      list.clear
      assert list.empty?
    end

    def test_insert
      list = LinkedList(Block).new
      list << block(0)
      list << block(2)
      list.insert(block(1), after: block(0))
      list.insert(block(3), after: block(2))
      assert_equal 4, list.size

      assert_equal block(1), block(0).value.next
      assert_equal block(2), block(1).value.next
      assert_equal block(3), block(2).value.next
      assert_equal Pointer(Block).null, block(3).value.next

      list << block(4)
      assert_equal 5, list.size
      assert_equal block(4), block(3).value.next
      assert_equal Pointer(Block).null, block(4).value.next
    end

    def test_each
      list = LinkedList(Block).new
      4.times { |i| list << block(i) }

      i = -1
      list.each do |item|
        i += 1
        assert_equal block(i), item
      end
    end
  end
end

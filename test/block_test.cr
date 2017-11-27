require "minitest/autorun"
require "../src/block"
require "../src/memory"

module GC
  class BlockTest < Minitest::Test
    @block : Pointer(Block)?

    def block
      @block ||= GC.map_and_align(SizeT.new(BLOCK_SIZE) * 2, SizeT.new(BLOCK_SIZE) * 2).as(Block*)
    end

    def setup
      # LibC.memset(block, 0, sizeof(Block))
      block.value.initialize
    end

    def test_limits
      assert_equal block.as(Void*) + LINE_SIZE, block.value.start
      assert_equal block.as(Void*) + BLOCK_SIZE, block.value.stop
    end

    def test_line_accessors
      header1 = block.value.line_header_at(62)
      header1.value.mark

      line = block.value.line_at(62)
      header2 = block.value.line_header_for(line)

      assert_equal header1, header2
      assert header2.value.marked?
    end

    def test_mark
      refute block.value.marked?

      block.value.mark
      assert block.value.marked?

      block.value.unmark
      refute block.value.marked?
    end

    def test_flag
      assert block.value.free?
      refute block.value.recyclable?
      refute block.value.unavailable?

      block.value.flag = Block::Flag::Recyclable
      refute block.value.free?
      assert block.value.recyclable?
      refute block.value.unavailable?

      block.value.flag = Block::Flag::Unavailable
      refute block.value.free?
      refute block.value.recyclable?
      assert block.value.unavailable?

      block.value.flag = Block::Flag::Free
      assert block.value.free?
      refute block.value.recyclable?
      refute block.value.unavailable?
    end
  end
end

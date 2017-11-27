require "minitest/autorun"
require "../src/line_header"

module GC
  class LineHeaderTest < Minitest::Test
    def test_marked
      line_header = LineHeader.new
      refute line_header.marked?

      line_header.mark
      assert line_header.marked?

      line_header.unmark
      refute line_header.marked?
    end

    def test_offset
      line_header = LineHeader.new
      refute line_header.contains_object_header?

      {0, 32, 248}.each do |offset|
        line_header.offset = offset
        assert line_header.contains_object_header?
        assert_equal offset, line_header.offset
      end
    end

    def test_clear
      line_header = LineHeader.new
      line_header.offset = 32
      line_header.mark

      assert line_header.contains_object_header?
      assert line_header.marked?

      line_header.clear
      refute line_header.contains_object_header?
      refute line_header.marked?
    end
  end
end

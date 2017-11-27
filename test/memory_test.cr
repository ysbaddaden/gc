require "minitest/autorun"
require "../src/memory"

module GC
  class MemoryTest < Minitest::Test
    def test_get_memory_size
      assert GC.get_memory_size > 0
    end
  end
end

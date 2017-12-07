require "minitest/autorun"
require "../src/chunk"

module GC
  class ChunkTest < Minitest::Test
    def test_initialize
      chunk = Chunk.new(SizeT.new(8192), Chunk::Flag::Free)
      assert_equal Chunk::Flag::Free, chunk.flag
      assert chunk.free?
      refute chunk.allocated?
      assert_equal 8192, chunk.size

      chunk = Chunk.new(SizeT.new(10_000), Chunk::Flag::Allocated)
      assert_equal Chunk::Flag::Allocated, chunk.flag
      refute chunk.free?
      assert chunk.allocated?
      assert_equal 10_000, chunk.size
    end

    def test_size
      chunk = Chunk.new(SizeT.new(8192), Chunk::Flag::Free)
      assert_equal 8192, chunk.size
      assert_equal 8192, chunk.@object.size

      chunk.size = SizeT.new(128)
      assert_equal 128, chunk.size
      assert_equal 128, chunk.@object.size
    end

    def test_mark
      chunk = Chunk.new(SizeT.new(8192), Chunk::Flag::Allocated)
      refute chunk.marked?
      refute chunk.object.marked?

      chunk.mark
      assert chunk.marked?
      assert chunk.object.marked?

      chunk.unmark
      refute chunk.marked?
      refute chunk.object.marked?
    end

    def test_includes?
      chunk = Chunk.new(SizeT.new(8192), Chunk::Flag::Allocated)
      refute chunk.includes?(pointerof(chunk).as(Void*))
      refute chunk.includes?(chunk.object_address.as(Void*))
      assert chunk.includes?(chunk.mutator_address)
      assert chunk.includes?(chunk.object_address.as(Void*) + 8191)
      refute chunk.includes?(chunk.object_address.as(Void*) + 8192)
    end
  end
end

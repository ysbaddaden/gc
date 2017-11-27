require "minitest/autorun"
require "../src/object"

module GC
  class ObjectTest < Minitest::Test
    @object = LibC.malloc(sizeof(Object) + 16).as(Object*)

    def teardown
      LibC.free(@object.as(Void*))
    end

    def test_initialize
      @object.value.initialize(SizeT.new(16), Object::Type::Standard)
      assert_equal 16, @object.value.size
      assert @object.value.standard?

      @object.value.initialize(SizeT.new(16384), Object::Type::Large)
      assert_equal 16384, @object.value.size
      assert @object.value.large?
    end

    def test_marked
      refute @object.value.marked?

      @object.value.mark
      assert @object.value.marked?

      @object.value.unmark
      refute @object.value.marked?
    end

    def test_mutator_address
      assert_equal (@object + 1).as(Void*), @object.value.mutator_address
    end
  end
end

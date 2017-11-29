module GC
  struct Chunk
    enum Flag
      Free
      Allocated
    end

    property next : Chunk*
    property flag : Flag
    getter object : Object

    def initialize(size : SizeT, @flag : Flag)
      @next = Pointer(Chunk).null
      @object = Object.new(size - sizeof(Chunk), Object::Type::Large)
    end

    delegate free?, allocated?, to: @flag
    delegate marked?, mark, unmark, size, to: @object

    def size=(value : SizeT)
      @object.size = value
    end

    def includes?(pointer : Void*) : Bool
      object_address.as(Void*) < pointer < object_address.as(Void*) + size
    end

    def object_address : Object*
      pointerof(@object)
    end

    def mutator_address : Void*
      @object.mutator_address
    end
  end
end

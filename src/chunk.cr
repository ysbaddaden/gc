require "./object"

module GC
  CHUNK_HEADER_SIZE = sizeof(Chunk) - sizeof(Object)

  @[Extern]
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
      @object = Object.new(size, Object::Type::Large)
    end

    delegate free?, allocated?, to: @flag
    delegate marked?, mark, unmark, to: @object

    # Reports the free available size, or the allocated object's size.
    def size
      @object.size
    end

    # Sets the free available size, or the allocated object's size.
    def size=(value : SizeT)
      @object.size = value
    end

    def includes?(pointer : Void*) : Bool
      mutator_address <= pointer < object_address.as(Void*) + size
    end

    def object_address : Object*
      pointerof(@object)
    end

    def mutator_address : Void*
      @object.mutator_address
    end
  end
end

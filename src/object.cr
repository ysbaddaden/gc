require "./constants"

module GC
  @[Extern]
  struct Object
    enum Type : UInt8
      Standard
      Large
    end

    @size : SizeT
    @type : Type
    @marked : UInt8

    def initialize(@size : SizeT, @type : Type)
      @marked = 0_u8
    end

    delegate standard?, large?, to: @type

    # Returns the object size including its header. Use `#allocation_size` to
    # get the available size.
    def size : SizeT
      @size
    end

    def size=(value : SizeT) : SizeT
      @size = value
    end

    # Returns the allocation available size.
    def allocation_size : SizeT
      @size - sizeof(Object)
    end

    def marked? : Bool
      @marked == 1_u8
    end

    def mark : Nil
      @marked = 1_u8
    end

    def unmark : Nil
      @marked = 0_u8
    end

    def mutator_address : Void*
      pointer_self.as(Void*) + sizeof(Object)
    end

    private def pointer_self
      pointerof(@size)
    end
  end
end

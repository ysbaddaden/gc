require "./constants"

module GC
  struct Object
    enum Type : UInt8
      Standard
      Large
    end

    @size : SizeT
    @type : Type
    @marked = 0_u8

    def initialize(@size : SizeT, @type : Type)
      @marked = 0_u8
    end

    delegate standard?, large?, to: @type

    def size : SizeT
      @size
    end

    def size=(value : SizeT) : SizeT
      @size = value
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

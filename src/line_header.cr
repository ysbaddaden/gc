require "./constants"

module GC
  alias Line = UInt8[LINE_SIZE]

  @[Extern]
  struct LineHeader
    # 0x 1        1       111111
    #    ^        ^       ^
    #    marked   object  offset/word size
    @flag = 0_u8

    def clear
      @flag = 0_u8
    end

    def contains_object_header? : Bool
      @flag.bit(6) == 1
    end

    def marked? : Bool
      @flag.bit(7) == 1
    end

    def mark
      @flag |= 0x80
    end

    def unmark
      @flag &= 0x7f
    end

    def offset : Int32
      # assert contains_object_header?
      (@flag & 0x3f).to_i32 * WORD_SIZE
    end

    def offset=(offset : Int) : Nil
      # assert offset % WORD_SIZE == 0
      # assert offset < 255 - WORD_SIZE
      @flag |= 0x40 | (offset / WORD_SIZE).to_u8
    end
  end
end

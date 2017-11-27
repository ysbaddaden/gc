require "./constants"
require "./line_header"

module GC
  # Immix Block.
  #
  # Actually a view into a pre-allocated block.
  #
  # Contains metadata information about the block, whether it's marked (or not)
  # and whether it's free (no objects allocated), recyclable (some lines have
  # allocated objects) or unavailable (all lines have objects).
  #
  # Also contains line metadata information, stored as a single array, distinct
  # from the actual lines. The line metadata contains information about whether
  # the line is marked (or not) has an object header (or not) and the offset
  # into the line at which the object header is.
  #
  # Eventually contains 127 lines of 256 bytes (that is 32 385 bytes) where
  # objects will be allocated. The block could fit 128 lines, but the first line
  # is reserved for block and line metadata.
  @[Extern]
  struct Block
    enum Flag : UInt8
      Free = 0
      Recyclable = 1
      Unavailable = 2
    end

    @marked : Bool
    @flag : Flag
    @first_free_line_index : Int16
    property next : Block*

    @line_headers = uninitialized LineHeader[LINE_COUNT]

    # 113-bytes on 64-bit with 8-byte word padding
    # 121-bytes on 32-bit with 4-byte word padding
    # @__padding = uninitialized UInt8[113]

    # @lines = uninitialized Line[LINE_COUNT]

    # Returns a pointer to the block a *pointer* points in.
    def self.from(pointer : Void*) : Block*
      Pointer(Void).new(pointer.address & BLOCK_SIZE_IN_BYTES_INVERSE_MASK).as(Block*)
    end

    def initialize
      @marked = false
      @flag = Flag::Free
      @first_free_line_index = 0_i16
      @next = Pointer(Block).null
    end

    delegate free?, recyclable?, unavailable?, to: @flag

    def marked? : Bool
      @marked
    end

    def mark : Nil
      @marked = true
    end

    def unmark : Nil
      @marked = false
    end

    def flag=(value : Flag)
      @flag = value
    end

    # Returns a pointer to the first free line.
    def first_free_line : Void*
      line_at(@first_free_line_index)
    end

    # Sets the first free line in the block.
    def first_free_line=(line : Void*) : Nil
      @first_free_line_index = line_index(line).to_i16
    end

    # Sets the first free line in the block.
    def first_free_line_index : Int32
      @first_free_line_index.to_i32
    end

    # Sets the first free line in the block.
    def first_free_line_index=(index : Int32) : Nil
      @first_free_line_index = index.to_i16
    end

    # Raw pointer to the line headers.
    private def line_headers : LineHeader*
      @line_headers.to_unsafe
    end

    # Iterates each `LineHeader` in the block.
    def each_line_header : Nil
      line_headers = @line_headers.to_unsafe
      LINE_COUNT.times do |index|
        yield line_headers + index, index
      end
    end

    # Iterates each `Object` in a `Line`. Doesn't cross to another line.
    #
    # NOTE: assumes the line does contain an object header!
    def each_object_at(line_index : Int32, line_header : LineHeader* = line_header_at(index)) : Nil
      line = line_at(line_index)
      offset = line_header.value.offset

      loop do
        object = (line + offset).as(Object*)
        break if (size = object.value.size) == 0

        yield object

        offset += size
        break if offset >= LINE_SIZE
      end
    end

    # Returns the index for a Line in the block given into the line.
    def line_index(line : Void*)
      (line.address - start.address) / LINE_SIZE
    end

    # Pointer to the `LineHeader` for a `Line` pointer.
    def line_header_for(line : Void*) : LineHeader*
      line_header_at(line_index(line))
    end

    # Pointer to the `LineHeader` at *index* in the block.
    def line_header_at(index : Int) : LineHeader*
      (line_headers + sizeof(LineHeader) * index).as(LineHeader*)
    end

    # Pointer to the `Line` at *index* in the block.
    def line_at(index : Int) : Void*
      (start + LINE_SIZE * index).as(Void*)
    end

    # Pointer to the start of the block (i.e. first line)
    def start : Void*
      pointer_self.as(Void*) + BLOCK_SIZE - LINE_SIZE * LINE_COUNT
    end

    # Pointer to end of the block (i.e. end of last line).
    def stop : Void*
      pointer_self.as(Void*) + BLOCK_SIZE
    end

    # Update the line header for an object that has been allocated into the
    # block. Records the offset at which the object has been saved in the line,
    # unless an object header was already allocated on the line, in which case
    # there is no need to update. The new object is just after another object
    # on the line and can be found.
    def update_line_header(object : Object*) : Nil
      index = line_index(object.as(Void*))
      line_header = line_header_at(index)

      unless line_header.value.contains_object_header?
        offset = (object.address - line_at(index).address).to_i
        line_header.value.offset = offset
      end
    end

    private def pointer_self : Block*
      pointerof(@marked).as(Block*)
    end
  end
end

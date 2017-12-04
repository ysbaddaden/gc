require "./constants"
require "./object"
require "./global_allocator"

module GC
  struct LocalAllocator
    def initialize(@global_allocator : GlobalAllocator*)
      @block = Pointer(Block).null
      @cursor = Pointer(Void).null
      @limit = Pointer(Void).null
    end

    def reset
      @block = Pointer(Block).null
      @cursor = Pointer(Void).null
      @limit = Pointer(Void).null
    end

    # Allocates an object. First tries to allocate in the current block
    # (previously fetched from the global allocator) then requests blocks from
    # the global allocator and tries again until the object can be allocated
    # properly.
    #
    # The global allocator is responsible for giving blocks from either the
    # recycled or free lists, then collect or grow the HEAP memory. The local
    # allocator merely requests blocks and tries to allocate in them.
    def allocate_small(size : SizeT) : Void*
      object_size = size + sizeof(Object)

      {% unless flag?(:release) %}
        abort "GC: allocated size must be a multiple of sizeof(Void*)" unless object_size % WORD_SIZE == 0
        abort "GC: allocated size must be smaller than 8KB" unless object_size < LARGE_OBJECT_SIZE
      {% end %}

      loop do
        # try to allocate into current block
        if @block
          object = try_allocate_small(object_size)

          unless object.null?
            GC.debug "malloc small size=%u object=%lu ptr=%lu stop=%lu",
              object_size, object.as(Void*), object.value.mutator_address, object.as(Void*) + object_size

            return object.value.mutator_address
          end
        end

        # failed to allocate: request next block from global allocator
        @block = @global_allocator.value.next_block
        GC.debug "next block=%lu stop=%lu flag=%d", @block, @block.as(Void*) + BLOCK_SIZE, @block.value.@flag.value

        init_cursors
      end
    end

    # Tries to bump allocate an object in the current block, skipping over holes
    # in the block that would be too small to fit the object.
    #
    # Returns a valid pointer if the allocation could fit, and a null pointer
    # otherwise, which means the current block was exhausted and a new block
    # must be requested from the global allocator.
    private def try_allocate_small(size : SizeT) : Object*
      loop do
        if @cursor + size <= @limit
          # 'allocate' object
          object = @cursor.as(Object*)
          object.value.initialize(size, Object::Type::Standard)
          @block.value.update_line_header(object)

          # update cursor to point after allocated object:
          @cursor += size

          unless @cursor == @limit
            # erase size field of next object (there can't be any object, but
            # there may be uninitialized or previously allocated data)
            @cursor.as(Word*).value = Word.new(0)
          end

          return object
        end

        unless set_cursors_to_next_hole_in_block
          # no more space in block
          return Pointer(Object).null
        end
      end
    end

    # Allocates a large objects (8KB and more).
    #
    # Large allocations are slower than smaller allocations, because they must
    # always reach to the `GlobalAllocator` (which will eventually require sync).
    def allocate_large(size : SizeT) : Void*
      object_size = size + sizeof(Object)

      {% unless flag?(:release) %}
        abort "GC: allocated size must be a multiple of sizeof(Void*)" unless size % WORD_SIZE == 0
        abort "GC: allocated size must be at least 8KB" unless size >= LARGE_OBJECT_SIZE
      {% end %}

      object = @global_allocator.value.allocate_large(object_size).as(Object*)

      GC.debug "malloc large size=%u object=%lu ptr=%lu stop=%lu",
        size, object.as(Void*), object.value.mutator_address, object.as(Void*) + size

      object.value.mutator_address
    end

    # Initializes `@cursor` and `@limit` for a newly fetched block from the
    # global allocator.
    private def init_cursors
      if @block.value.free?
        @cursor = @block.value.start
        @limit = @block.value.stop
        return
      end

      if @block.value.recyclable?
        @cursor = @block.value.first_free_line
        @limit = find_cursor_limit
        return
      end

      abort "GC: can't init cursors into unavailable block"
    end

    # Searches the next hole in the block. Cursor will be initialized to the
    # next free line after the current limit, then limit will be set to the
    # next marked line in the block after the new cursor, or to the end of the
    # block.
    #
    # Returns true if a hole was found and `@cursor` and `@limit` were updated
    # accordingly. Otherwise returns false and a new block should be requested
    # from the global allocator.
    private def set_cursors_to_next_hole_in_block : Bool
      GC.debug "search hole block=%lu cursor=%lu limit=%lu", @block, @cursor, @limit

      stop = @block.value.stop
      line = @limit
      free = false

      loop do
        line += LINE_SIZE
        return false if line >= stop

        if @block.value.line_header_for(line).value.marked?
          free = false
        elsif free
          @cursor = line
          @limit = find_cursor_limit
          GC.debug "found hole block=%lu cursor=%lu limit=%lu", @block, @cursor, @limit
          return true
        else
          # found a free line, but we skip it because of conservative marking
          # where a small object may span a line, but we didn't precisely mark
          # all lines (improves performance).
          free = true
        end
      end
    end

    # Given the current cursor, finds the limit for the hole, that is the first
    # marked line after the cursor or the end of the block if all lines are
    # free after the cursor.
    private def find_cursor_limit : Void*
      stop = @block.value.stop
      line = @cursor

      loop do
        line += LINE_SIZE
        if line == stop
          return stop
        end

        if @block.value.line_header_for(line).value.marked?
          return line
        end
      end
    end
  end
end

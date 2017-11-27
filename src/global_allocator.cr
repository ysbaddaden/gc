require "./constants"
require "./memory"
require "./block_list"
require "./utils"
require "./collector"

module GC
  struct GlobalAllocator
    @@memory_limit = GC.get_memory_size

    getter heap_size : SizeT
    @heap_start : Void*
    @heap_stop : Void*

    @recycled_list : BlockList
    @free_list : BlockList

    def initialize(initial_size : SizeT) : Nil
      unless initial_size >= BLOCK_SIZE * 2
        # it will eventually need at least 2 free blocks per local allocator
        # (i.e. one for each thread running fibers), one for bump allocating
        # objects and another one for overflow allocations (not implemented).
        abort "GC: initial size must be at least twice the block size (64KB)"
      end

      unless initial_size % BLOCK_SIZE == 0
        abort "GC: initial size must be a multiple of block size (32KB)"
      end

      # initial heap pointer
      start = GC.map_and_align(@@memory_limit, initial_size)
      stop = start + initial_size

      @heap_size = initial_size
      @heap_start = start
      @heap_stop = stop

      GC.debug "heap size=%u start=%lu stop=%lu", @heap_size, @heap_start, @heap_stop

      # recycled blocks list (empty)
      @recycled_list = BlockList.new

      # free blocks list (initial_size / BLOCK_SIZE)
      @free_list = BlockList.new

      block = start
      until block == stop
        @free_list << block.as(Block*)
        block += BLOCK_SIZE
      end
    end

    def in_heap?(pointer : Void*) : Bool
      in_small_heap?(pointer) || in_large_heap?(pointer)
    end

    def in_small_heap?(pointer : Void*) : Bool
      @heap_start < pointer < @heap_stop
    end

    def in_large_heap?(pointer : Void*) : Bool
      false
    end

    def each_block
      cursor = @heap_start
      until cursor == @heap_stop
        yield cursor.as(Block*)
        cursor += BLOCK_SIZE
      end
    end

    # Request the next available block to allocate objects to. First tries to
    # return a recyclable block then a free block. If no block is available,
    # triggers a collection, then tries again to return a recyclable then free
    # block. In the event that there are still no block available, grows the
    # HEAP memory and returns a free block.
    def next_block : Block*
      # TODO: synchronize accesses to: each list, to collect, and to grow

      # 1. exhaust recycled list:
      block = @recycled_list.shift
      return block unless block.null?

      # 2. exhaust free list:
      block = @free_list.shift
      return block unless block.null?

      # 3. no blocks? collect!
      GC.collect

      # 4. exhaust freshly recycled list:
      block = @recycled_list.shift
      return block unless block.null?

      # 5. still no blocks? grow!
      if @free_list.empty?
        grow
      end

      # 6. exhaust free list
      block = @free_list.shift
      return block unless block.null?

      # 7. seriously, no luck
      abort "GC: failed to shift block from free list"
    end

    def recycle : Nil
      @recycled_list.clear
      @free_list.clear

      each_block do |block|
        if block.value.marked?
          recycle(block)
        else
          free(block)
        end
      end
    end

    # Tries to find a free line in the block, taking care to skip a free line to
    # account for conservative marking (a small object may span a line, but the
    # collector didn't check and marked the following line to improve
    # performance).
    private def recycle(block : Block*) : Nil
      free = false

      block.value.each_line_header do |line_header, line_index|
        if line_header.value.marked?
          free = false
        elsif free || line_index == 0
          # free line is first in block or follows another free line: it's safe to use
          recyclable(block, line_index)
          return
        else
          # found a free line, but skip it
          free = true
        end
      end

      # the block is full
      unavailable(block)
    end

    def recyclable(block : Block*, first_free_line_index : Int32 = -1) : Nil
      block.value.flag = Block::Flag::Recyclable
      block.value.first_free_line_index = first_free_line_index
      @recycled_list << block
    end

    def free(block : Block*) : Nil
      block.value.flag = Block::Flag::Free
      #block.value.first_free_line = 0
      @free_list << block
    end

    def unavailable(block : Block*) : Nil
      block.value.flag = Block::Flag::Unavailable
      #block.value.first_free_line = -1
    end

    private def grow : Nil
      increment = BLOCK_SIZE
      abort "GC: out of memory" if @heap_size + increment >= @@memory_limit

      GC.debug("growing HEAP by %zu bytes to %zu bytes", increment, @heap_size + increment)

      block = @heap_stop
      stop = @heap_stop += increment

      until block == stop
        @free_list << block.as(Block*)
        block += BLOCK_SIZE
      end

      @heap_size += increment
    end
  end
end

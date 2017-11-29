require "./constants"
require "./memory"
require "./block"
require "./linked_list"
require "./chunk"
require "./utils"
require "./collector"

module GC
  struct GlobalAllocator
    @@memory_limit = GC.get_memory_size

    getter heap_size : SizeT
    @heap_start : Void*
    @heap_stop : Void*

    @recycled_list : LinkedList(Block)
    @free_list : LinkedList(Block)

    getter large_heap_size : SizeT
    @large_heap_start : Void*
    @large_heap_stop : Void*

    @chunks : LinkedList(Chunk)

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

      @recycled_list = LinkedList(Block).new
      @free_list = LinkedList(Block).new

      cursor = start
      until cursor == stop
        block = cursor.as(Block*)
        block.value.initialize
        @free_list << block
        cursor += BLOCK_SIZE
      end

      # initial large heap pointer
      large_start = GC.map_and_align(@@memory_limit, initial_size)
      large_stop = large_start + initial_size

      @large_heap_size = initial_size
      @large_heap_start = large_start
      @large_heap_stop = large_stop

      GC.debug "heap size=%u start=%lu stop=%lu large_heap_size=%d large_start=%lu large_stop=%lu",
        @heap_size, @heap_start, @heap_stop, @large_heap_size, @large_heap_start, @large_heap_stop

      @chunks = LinkedList(Chunk).new
      chunk = large_start.as(Chunk*)
      chunk.value.initialize(initial_size, Chunk::Flag::Free)
      @chunks << chunk
    end

    def in_heap?(pointer : Void*) : Bool
      in_small_heap?(pointer) || in_large_heap?(pointer)
    end

    def in_small_heap?(pointer : Void*) : Bool
      @heap_start < pointer < @heap_stop
    end

    def in_large_heap?(pointer : Void*) : Bool
      @large_heap_start < pointer < @large_heap_stop
    end

    def each_block
      cursor = @heap_start
      until cursor == @heap_stop
        yield cursor.as(Block*)
        cursor += BLOCK_SIZE
      end
    end

    def each_chunk
      @chunks.each { |chunk| yield chunk }
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
      abort "GC: failed to allocate small object (can't shift block from free list)"
    end

    def allocate_large(object_size : SizeT) : Object*
      # TODO: synchronize

      # 1. search suitable chunk:
      object = try_allocate_large(object_size)
      return object unless object.null?

      # 2. no space? collect!
      GC.collect

      # 3. search suitable chunk (again):
      object = try_allocate_large(object_size)
      return object unless object.null?

      # 4. still no space? grow large heap
      grow_large(object_size + sizeof(Chunk))

      # 5. allocate:
      object = try_allocate_large(object_size)
      return object unless object.null?

      # 6. seriously, no luck
      abort "GC: failed to allocate large object"
    end

    def try_allocate_large(size : SizeT) : Object*
      i = 0

      @chunks.each do |chunk|
        next unless chunk.value.free?

        if size == chunk.value.size
          chunk.value.flag = Chunk::Flag::Allocated
          chunk.value.size = size
          return chunk.value.object_address

        elsif size < chunk.value.size
          remaining = chunk.value.size - size
          free = (chunk.as(Void*) + sizeof(Chunk) + size).as(Chunk*)
          free.value.initialize(remaining, Chunk::Flag::Free)
          @chunks.insert(free, after: chunk)

          chunk.value.flag = Chunk::Flag::Allocated
          chunk.value.size = size
          return chunk.value.object_address
        end
      end

      Pointer(Object).null
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

      each_chunk do |chunk|
        next if chunk.value.marked?
        chunk.value.flag = Chunk::Flag::Free
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
      block.value.first_free_line_index = 0
      @free_list << block
    end

    def unavailable(block : Block*) : Nil
      block.value.flag = Block::Flag::Unavailable
      block.value.first_free_line_index = -1
    end

    private def grow : Nil
      # TODO: grow the small heap more broadly
      increment = SizeT.new(BLOCK_SIZE)
      out_of_memory(increment)

      GC.debug "growing HEAP by %zu bytes to %zu bytes", increment, @heap_size + increment

      cursor = @heap_stop
      stop = @heap_stop += increment
      @heap_size += increment

      until cursor == stop
        block = cursor.as(Block*)
        block.value.initialize
        @free_list << block
        cursor += BLOCK_SIZE
      end
    end

    private def grow_large(increment : SizeT) : Nil
      increment = SizeT.new(1) << Math.log2(increment).ceil.to_u64
      increment = GC.round_to_next_multiple(increment, BLOCK_SIZE)
      out_of_memory(increment)

      GC.debug "growing large HEAP by %zu bytes to %zu bytes", increment, @large_heap_size + increment

      current = @large_heap_stop
      @large_heap_stop += increment
      @large_heap_size += increment

      chunk = current.as(Chunk*)
      chunk.value.initialize(increment, Chunk::Flag::Free)
      @chunks << chunk
    end

    private def out_of_memory(increment : SizeT) : Nil
      if @heap_size + @large_heap_size + increment >= @@memory_limit
        abort "GC: out of memory"
      end
    end
  end
end

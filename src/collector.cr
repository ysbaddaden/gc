require "./global_allocator"
require "./object"

class Fiber
  # :nodoc:
  def self.each : Nil
    fiber = @@first_fiber
    while fiber
      yield fiber
      fiber = fiber.next_fiber
    end
  end
end

module GC
  struct Collector
    # TODO: there should be many local allocators, and they should be associated to threads instead.
    # OPTIMIZE: consider having a pool of threads to visit stacks & mark objects in parallel

    def initialize(@global_allocator : GlobalAllocator*, @local_allocator : LocalAllocator*)
    end

    # Collects memory, in a stop the world fashion.
    def collect
      # TODO: synchronize & prevent any allocation while the collector is running!

      fiber = Fiber.current

      # We collect in a new fiber, so the stack bottom of the current fiber will
      # be correct and CPU registers saved to the stack.
      #
      # This doesn't apply to the new fiber, since the garbage collector doesn't
      # use itself to allocate memory, so this fiber's stack doesn't have any
      # references to heap memory (and will be skipped entirely).
      spawn do
        GC.debug ""

        # unmark objects in small and large object spaces:
        unmark_all

        # TODO: implement a precise stack iterator with LLVM GC / STACK MAPS
        #       allowing to implement moving/compact/defragment IMMIX optimizations
        each_stack do |top, bottom|
          mark_from_region(top, bottom)
        end

        # recycle blocks then reset allocator cursors:
        @global_allocator.value.recycle
        @local_allocator.value.reset

        fiber.resume
      end

      Scheduler.reschedule
    end

    private def unmark_all
      @global_allocator.value.each_block do |block|
        # GC.debug "unmark block=%lu", block
        block.value.unmark

        block.value.each_line_header do |line_header, line_index|
          # GC.debug "unmark block=%lu line=%d", block, line_index if line_header.value.marked?
          line_header.value.unmark

          block.value.each_object_at(line_index, line_header) do |object|
            # GC.debug "unmark block=%lu object=%lu", block, object.as(Void*)
            object.value.unmark
          end
        end
      end

      @global_allocator.value.each_chunk do |chunk|
        # GC.debug "unmark chunk=%lu", chunk
        chunk.value.unmark
      end
    end

    private def mark_from_region(stack_pointer : Void*, stack_bottom : Void*)
      # assert stack_pointer <= stack_bottom

      GC.debug "mark_from_region sp=%lu bottom=%lu", stack_pointer, stack_bottom

      cursor = stack_pointer
      while cursor < stack_bottom
        pointer = Pointer(Void).new(cursor.as(Word*).value)

        unless pointer.null?
          # GC.debug "mark_from_region cursor=%lu value=%lu", cursor, pointer

          if @global_allocator.value.in_small_heap?(pointer)
            mark_small(pointer)
          elsif @global_allocator.value.in_large_heap?(pointer)
            mark_large(pointer)
          end
        end

        cursor += sizeof(Void*)
      end
    end

    private def mark_small(pointer : Void*) : Nil
      GC.debug "find object ptr=%lu", pointer

      block = @global_allocator.value.block_for(pointer)

      # find the line the object is allocated to —this may be an inner pointer,
      # and the object may span one (small) or many (medium) lines:
      line_index = block.value.line_index(pointer)
      line_header = block.value.line_header_at(line_index)

      # search lines until we find one with an allocated object:
      loop do |i|
        if line_index < 0
          GC.debug "failed to find allocation line for heap pointer, line_index=%d", line_index
          return
        end

        if line_header.value.contains_object_header?
          if i == 0 && block.value.line_at(line_index) + line_header.value.offset > pointer
            # the line the pointer is in has an object header but it's located
            # after the pointer, thus can't be its object
          else
            break
          end
        end

        line_index -= 1
        line_header = block.value.line_header_at(line_index)
      end

      # find the object (somewhere in line):
      line_offset = line_header.value.offset
      line = block.value.line_at(line_index)
      object = uninitialized Object*

      loop do
        cursor = line + line_offset

        object = cursor.as(Object*)
        size = object.value.size

        unless 0 < size < LARGE_OBJECT_SIZE
          GC.debug "failed to find object for heap pointer at line_index=%d", line_index
          return
        end

        break if cursor + size >= pointer
        line_offset += size

        if line_offset >= LINE_SIZE
          GC.debug "failed to find object for heap pointer at line_index=%d", line_index
          return
        end
      end

      # don't mark twice (avoid circular references)
      if object.value.marked?
        GC.debug "skip marked object=%lu", object.as(Void*)
        return
      end

      GC.debug "mark object=%lu ptr=%lu", object.as(Void*), pointer
      object.value.mark

      GC.debug "mark block=%lu", block
      block.value.mark

      # small object: conservative marking (we only mark the line, the
      # allocator will skip the next line, since a small object may span)
      GC.debug "mark block=%lu line=%d", block, line_index
      line_header.value.mark

      # medium object: exact marking (mark all lines the object spans to, except
      # for the last one, since we'll skip a free line anyway)
      if object.value.size >= LINE_SIZE
        GC.debug "mark block=%lu line=%d", block, (line_index += 1)
        spans = (object.value.size - line_offset) / LINE_SIZE
        spans.times { (line_header += 1).value.mark }
      end

      # conservative collector: scan the object for heap pointers
      bottom = object.as(Void*) + object.value.size
      mark_from_region(object.value.mutator_address, bottom)
    end

    private def mark_large(pointer : Void*) : Nil
      # search chunk for large object (may be inner pointer)
      @global_allocator.value.each_chunk do |chunk|
        next unless chunk.value.includes?(pointer)

        # don't mark object twice (avoid circular references)
        if chunk.value.marked?
          GC.debug "skip marked object=%lu", chunk.value.object_address.as(Void*)
          return
        end

        GC.debug "mark object=%lu ptr=%lu", chunk.value.object_address.as(Void*), pointer
        chunk.value.mark

        # conservative collector: scan the object for heap pointers
        bottom = chunk.value.object_address.as(Void*) + chunk.value.size
        mark_from_region(chunk.value.mutator_address, bottom)

        return
      end
    end

    # Iterates the stack of all fibers, except for the stack of the current
    # fiber, which is expected to be a stack dedicated to the garbage collector,
    # thus a stack that doesn't use the GC allocator to allocate memory.
    private def each_stack
      current = Fiber.current

      Fiber.each do |fiber|
        next if fiber == current
        yield fiber.@stack_top, fiber.@stack_bottom
      end
    end
  end
end

require "./constants"
require "./global_allocator"
require "./local_allocator"
require "./collector"

module GC
  @@global_allocator = GlobalAllocator.new(SizeT.new(BLOCK_SIZE) * 128)
  @@local_allocator = LocalAllocator.new(pointerof(@@global_allocator))
  @@collector = Collector.new(pointerof(@@global_allocator), pointerof(@@local_allocator))

  def self.init
  end

  def self.enable
  end

  def self.disable
  end

  def self.is_heap_ptr(pointer : Void*)
    @@global_allocator.is_heap?(pointer)
  end

  def self.malloc(size : SizeT) : Void*
    size = round_to_next_multiple(size, WORD_SIZE)
    if size < (LARGE_OBJECT_SIZE - sizeof(Object))
      @@local_allocator.allocate_small(size)
    else
      @@local_allocator.allocate_large(size)
    end
  end

  def self.malloc_atomic(size : SizeT) : Void*
    # TODO: pass atomic flag to local allocator, to avoid conservative marking
    #       of objects that don't reference HEAP pointers (e.g. buffers).
    malloc(size)
  end

  # Changes the size of a memory allocation. The original pointer must have been
  # allocated using `#malloc` or `#malloc_atomic` or reallocated with
  # `#realloc`. The new memory allocation will be initialized with the data from
  # the previous pointer (up to the minimum of the old or new size).
  #
  # Returns a pointer to the new memory allocation; the original pointer musn't
  # be accessed or referenced anymore.
  def self.realloc(pointer : Void*, size : SizeT) : Void*
    # realloc(3) compatibility
    return malloc(size) if pointer.null?

    new_size = round_to_next_multiple(size, WORD_SIZE)

    # get original object (for actual allocation size)
    object = (pointer - sizeof(Object)).as(Object*)
    original_size = object.value.allocation_size

    # downsize? do nothing
    return pointer if new_size <= original_size

    # malloc + copy data:
    new_pointer = malloc(size)
    pointer.copy_to(new_pointer, original_size)

    # free large object:
    if original_size >= LARGE_OBJECT_SIZE
      @@global_allocator.deallocate_large(pointer)
    end

    new_pointer
  end

  def self.free(pointer : Void*) : Nil
    if @@global_allocator.in_large_heap?(pointer)
      @@global_allocator.deallocate_large(pointer)
    end
  end

  def self.collect
    @@collector.collect
  end

  def self.add_finalizer(object : Reference)
    add_finalizer_impl(object)
  end

  def self.add_finalizer_impl(object : T) forall T
    LibC.printf "WARNING: Immix GC doesn't support finalizers (yet)\n"
    # proc = ->(obj : Void*) { Box(T).unbox(obj).finalize }
    # @@global_allocator.register_finalizer(Box(T).box(object), proc)
    nil
  end

  def self.add_finalizer(any)
    # nothing
  end

  # :nodoc:
  def self.pthread_create(thread : LibC::PthreadT*, attr : Void*, start : Void* ->, arg : Void*)
    # TODO: attach thread stack to collector
    LibC.pthread_create(thread, attr, wrap, arg)
  end

  # :nodoc:
  def self.pthread_join(thread : LibC::PthreadT, value : Void**)
    LibC.pthread_join(thread, value)
  end

  # :nodoc:
  def self.pthread_detach(thread : LibC::PthreadT)
    # TODO: detach thread stack from collector
    LibC.pthread_detach(thread)
  end

  def self.stats
    zero = LibC::ULong.new(0)
    heap_size = LibC::ULong.new(@@global_allocator.heap_size + @@global_allocator.large_heap_size)
    Stats.new(heap_size, zero, zero, zero, zero)
  end

  # :nodoc:
  def self.push_stack(top : Void*, bottom : Void*)
  end

  # :nodoc:
  def self.before_collect(&block)
  end
end

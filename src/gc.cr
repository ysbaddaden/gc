require "./constants"
require "./global_allocator"
require "./local_allocator"
require "./collector"

module GC
  @@global_allocator : GlobalAllocator
  @@collector : Collector
  @@local_allocator = Pointer(LocalAllocator).null

  def self.init(initial_size : Int = DEFAULT_INITIAL_SIZE)
    @@global_allocator = GlobalAllocator.new(SizeT.new(initial_size))
    @@collector = Collector.new(pointerof(@@global_allocator))

    @@local_allocator = LibC.malloc(sizeof(LocalAllocator)).as(LocalAllocator*)
    @@local_allocator.value.initialize(pointerof(@@global_allocator))
  end

  def self.enable
  end

  def self.disable
  end

  def self.is_heap_ptr(pointer : Void*)
    @@global_allocator.is_heap?(pointer)
  end

  def self.malloc(size : SizeT) : Void*
    object_size = round_to_next_multiple(size, WORD_SIZE)
    if object_size < LARGE_BLOCK_SIZE
      @@local_allocator.value.allocate_small(object_size)
    else
      @@local_allocator.value.allocate_large(object_size)
    end
  end

  def self.malloc_atomic(size : SizeT) : Void*
    malloc(size)
  end

  def self.realloc(pointer : Void*, size : SizeT) : Void*
    raise "GC: pointer reallocation isn't implemented"
  end

  # def self.free(pointer : Void*)
  # end

  def self.collect
    @@collector.collect
  end

  def self.add_finalizer(object : Reference)
    # TODO: support Reference finalizers
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
    heap_size = LibC::ULong.new(@@global_allocator.heap_size)
    Stats.new(heap_size, zero, zero, zero, zero)
  end

  # :nodoc:
  def self.push_stack(top : Void*, bottom : Void*)
  end

  # :nodoc:
  def self.before_collect(&block)
  end
end

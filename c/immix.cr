@[Link("pthread")]
@[Link("m")]
@[Link(ldflags: "#{__DIR__}/immix.a")]
lib LibC
  fun GC_init(SizeT) : Void
  fun GC_malloc(SizeT) : Void*
  fun GC_malloc_atomic(SizeT) : Void*
  fun GC_realloc(Void*, SizeT) : Void*
  fun GC_free(Void*) : Void
  fun GC_in_heap(Void*) : Int

  alias CollectCallbackT = ->
  fun GC_register_collect_callback(CollectCallbackT) : Int
  fun GC_collect_once() : Void
  fun GC_mark_region(Void*, Void*, Char*) : Void

  # alias FinalizerT = Void* -> Nil
  # fun GC_register_finalizer(Void*, FinalizerT) : Int

  {% if flag?(:gnu) %}
    $__libc_stack_end : Void*
  {% end %}

  fun abort
end

fun gc_collect = GC_collect
  GC.collect
end

module GC
  @@pending : Fiber?
  @@collector : Fiber?

  def self.init : Nil
    #LibC.GC_init(4 * 1024 * 1024);
    LibC.GC_init(32768 * 2);
    @@collector = spawn(name: "GC_IMMIX_COLLECTOR") { collector_loop }
  end

  def self.enable
  end

  def self.disable
  end

  def self.malloc(size : LibC::SizeT) : Void*
    LibC.GC_malloc(size)
  end

  def self.malloc_atomic(size : LibC::SizeT) : Void*
    LibC.GC_malloc_atomic(size)
  end

  def self.realloc(pointer : Void*, size : LibC::SizeT) : Void*
    LibC.GC_realloc(pointer, size)
  end

  def self.free(pointer : Void*) : Nil
    LibC.GC_free(pointer)
  end

  def self.collect : Nil
    @@pending = Fiber.current
    @@collector.try(&.resume)
  end

  protected def self.collector_loop
    Scheduler.reschedule

    loop do
      begin
        LibC.GC_collect_once()

        if pending = @@pending
          @@pending = nil
          pending.resume
        else
          Scheduler.reschedule
        end
      rescue
        LibC.dprintf(2, "GC: collector crashed\n")
        LibC.abort()
      end
    end
  end


  def self.is_heap_ptr(pointer : Void*)
    LibC.GC_in_heap(pointer) == 1
  end

  def self.add_finalizer(object : Reference) : Nil
    add_finalizer_impl(object)
  end

  def self.add_finalizer_impl(object : T) forall T
    # TODO: register finalizers
    # LibC.GC_register_finalizer(object.as(Void*), ->(obj : Void*) {
    #   obj.as(T).finalize
    #   nil
    # })
  end

  def self.add_finalizer(object)
  end

  def self.stats
    # TODO: stats
    zero = LibC::ULong.new(0)
    Stats.new(zero, zero, zero, zero, zero)
  end

  # :nodoc:
  def self.pthread_create(thread : LibC::PthreadT*, attr : LibC::PthreadAttrT*, start : Void* -> Void*, arg : Void*)
    LibC.pthread_create(thread, attr, start, arg)
  end

  # :nodoc:
  def self.pthread_join(thread : LibC::PthreadT) : Void*
    ret = LibC.pthread_join(thread, out value)
    raise Errno.new("pthread_join") unless ret == 0
    value
  end

  # :nodoc:
  def self.pthread_detach(thread : LibC::PthreadT)
    LibC.pthread_detach(thread)
  end

  # :nodoc:
  def self.stack_bottom : Void*
    {% if flag?(:linux) %}
      {% if flag?(:gnu) %}
        return LibC.__libc_stack_end
      {% end %}
    {% end %}

    {% raise "GC: unsupported target (only <arch>-linux-gnu is supported)" %}
  end

  # :nodoc:
  def self.stack_bottom=(stack_bottom)
  end

  # :nodoc:
  def self.push_stack(stack_top, stack_bottom)
    LibC.GC_mark_region(stack_top, stack_bottom, "fiber")
  end

  # :nodoc:
  def self.before_collect(&block)
    LibC.GC_register_collect_callback(block)
  end
end

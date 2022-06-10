require "./lib_immix"

fun gc_collect = GC_collect
  GC.collect
end

module GC
  @@lock = Atomic::Flag.new
  @@pending : Fiber?
  @@collector : Fiber = spawn(name: "GC_IMMIX_COLLECTOR") { collector_loop }

  def self.init : Nil
    LibC.GC_init
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
    if @@lock.test_and_set
      @@pending = Fiber.current
      @@collector.resume
      @@lock.clear
    else
      Fiber.yield
    end
  end

  protected def self.collector_loop
    sleep

    loop do
      begin
        LibC.GC_collect_once()

        if pending = @@pending
          @@pending = nil
          pending.resume
        else
          # shouldn't happen since GC.collect always sets @@pending
          sleep
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

  private def self.add_finalizer_impl(object : T) forall T
    LibC.GC_register_finalizer(object.as(Void*), ->(obj : Void*) {
      obj.as(T).finalize
      nil
    })
  end

  def self.add_finalizer(object)
  end

  def self.register_disappearing_link(pointer : Void**)
    # FIXME
  end

  def self.stats
    zero = LibC::ULong.new(0)
    Stats.new(LibC.GC_get_heap_usage, zero, zero, zero, zero)
  end

  # :nodoc:
  def self.pthread_create(thread : LibC::PthreadT*, attr : LibC::PthreadAttrT*, start : Void* -> Void*, arg : Void*)
    LibC.pthread_create(thread, attr, ->(value) {
      LibC.GC_init_thread()
      start.call(value)
    }, arg)
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
  def self.current_thread_stack_bottom
    {% if flag?(:linux) && flag?(:gnu) %}
      return {Pointer(Void).null, LibC.__libc_stack_end}
    {% else %}
      {% raise "GC: unsupported target (only <arch>-linux-gnu is supported)" %}
    {% end %}
  end

  # :nodoc:
  def self.set_stackbottom(stack_bottom : Void*)
  end

  # :nodoc:
  def self.lock_read
  end

  # :nodoc:
  def self.unlock_read
  end

  # :nodoc:
  def self.lock_write
  end

  # :nodoc:
  def self.unlock_write
  end

  # :nodoc:
  def self.push_stack(stack_top, stack_bottom)
    LibC.GC_add_roots(stack_top, stack_bottom, "fiber")
  end

  # :nodoc:
  def self.before_collect(&block)
    LibC.GC_register_collect_callback(block)
  end

  GC.before_collect do
    Fiber.unsafe_each do |fiber|
      fiber.push_gc_roots unless fiber.running?
    end
  end
end

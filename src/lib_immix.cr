@[Link("pthread")]
@[Link("m")]
@[Link(ldflags: "#{__DIR__}/../immix.a")]
lib LibC
  fun GC_init() : Void
  fun GC_init_thread() : Void
  fun GC_malloc(SizeT) : Void*
  fun GC_malloc_atomic(SizeT) : Void*
  fun GC_realloc(Void*, SizeT) : Void*
  fun GC_free(Void*) : Void
  fun GC_in_heap(Void*) : Int

  fun GC_lock() : Void
  fun GC_unlock() : Void

  alias GC_CollectCallbackT = ->
  fun GC_register_collect_callback(GC_CollectCallbackT) : Int
  fun GC_collect_once() : Void
  fun GC_is_collecting() : Int32
  fun GC_add_roots(Void*, Void*, Char*) : Void

  #fun GC_print_stats() : Void
  fun GC_get_memory_use() : SizeT
  fun GC_get_heap_usage() : SizeT

  alias GC_FinalizerT = Void* -> Nil
  fun GC_register_finalizer(Void*, GC_FinalizerT) : Int

  {% if flag?(:linux) && flag?(:gnu) %}
    $__libc_stack_end : Void*
  {% elsif flag?(:darwin) %}
    fun pthread_get_stackaddr_np(x0 : PthreadT) : Void*
  {% end %}

  fun abort() : Void
end

lib LibC
  {% if flag?(:linux) %}
    MAP_NORESERVE = 0x4000
    SC_PAGE_SIZE = 30
    SC_PHYS_PAGES = 85

    {% if flag?(:gnu) %}
      $__libc_stack_end : Void*
    {% end %}

  {% else %}
    {% raise "GC: unsupported platform (LibC)" %}
  {% end %}

  fun memset(Void*, Int, SizeT) : Void*
end

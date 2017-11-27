module GC
  protected def self.round_to_next_multiple(size, multiple)
    (size + multiple - 1) / multiple * multiple
  end

  # :nodoc:
  def self.stack_bottom
    {% if flag?(:gnu) %}
      LibC.__libc_stack_end
    {% elsif flag?(:linux) %}
      # TODO: read stackstart from /proc/self/stat (28th integer)
      #       using only LibC (nothing from core/stdlib that rely on GC!)
    {% else %}
      # TODO: support the Darwin, FreeBSD, OpenBSD, ... platforms
      #
      #       register a custom segfault handler
      #       walk the stack up until segfault
      #         get stack pointer
      #         resume operation
      #       unregister segfault handler
    {% end %}
  end

  # :nodoc:
  def self.stack_bottom=(value : Void*)
  end

  macro debug(message, *args)
    {% if flag?(:GC_DEBUG) %}
      {% if args.size > 0 %}
        LibC.printf("GC: {{message.id}}\n", {{args.splat}})
      {% elsif message.size > 0 %}
        LibC.printf("GC: {{message.id}}\n")
      {% else %}
        LibC.printf("\n")
      {% end %}
    {% end %}
  end
end

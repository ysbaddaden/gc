module GC
  alias SizeT = LibC::SizeT

  # NOTE: actually IntptrT
  {% if flag?(:bits64) %}
    alias Word = UInt64
  {% elsif flag?(:bits32) %}
    alias Word = UInt32
  {% else %}
    {% raise "GC: unsupported platform (neither 32 nor 64-bit)"%}
  {% end %}
end

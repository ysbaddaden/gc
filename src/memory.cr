require "./constants"
require "./lib_c"

module GC
  MEM_PROT = LibC::PROT_READ | LibC::PROT_WRITE
  MEM_FLAGS = LibC::MAP_NORESERVE | LibC::MAP_PRIVATE | LibC::MAP_ANONYMOUS
  MEM_FD = -1
  MEM_OFFSET = 0

  def self.map_and_align(memory_limit : SizeT, alignment_size : SizeT) : Void*
    start = LibC.mmap(nil, memory_limit, MEM_PROT, MEM_FLAGS, MEM_FD, MEM_OFFSET)
    abort "GC: failed to mmap memory" if start.address == -1

    alignment_mask = SizeT.new(~(alignment_size - 1))
    address = Word.new(start.address)

    unless address & alignment_mask == address
      previous_block = Pointer(Void).new(address & BLOCK_SIZE_IN_BYTES_INVERSE_MASK)
      start = previous_block + BLOCK_SIZE
    end

    start.as(Void*)
  end

  def self.get_memory_size : SizeT
    {% if flag?(:darwin) || flag?(:freebsd) || flag?(:openbsd) %}
      mib = uninitialized LibC::Int[2]
      mib[0] = LibC::CTL_HW

      {% if flag?(:darwin) %}
        mib[1] = LibC::HW_MEMSIZE
        size = 0_i64
      {% elseif flag?(:freebsd) %}
        mib[1] = LibC::HW_PHYSMEM
        size = 0_i32
      {% elsif flag?(:openbsd) %}
        mib[1] = LibC::HW_PHYSMEM64
        size = 0_i64
      {% end %}

      len = LibC::SizeT.new(sizeof(typeof(size)))
      if LibC.sysctl(mib, pointerof(size), pointerof(len), nil, 0) == 0
        return LibC::SizeT.new(size)
      end

    {% elsif flag?(:linux) %}
      page_size = LibC.sysconf(LibC::SC_PAGE_SIZE)
      return LibC::SizeT.new(LibC.sysconf(LibC::SC_PHYS_PAGES)) * page_size

    {% else %}
      {% raise "GC: unsupported platform (failed to get physical memory size)" %}
    {% end %}

    LibC::SizeT.new(0)
  end
end

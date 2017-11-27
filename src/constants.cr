require "./types"

module GC
  WORD_SIZE = sizeof(Word)

  BLOCK_SIZE = 32768
  BLOCK_SIZE_IN_BYTES_INVERSE_MASK = ~(BLOCK_SIZE - 1)

  LINE_SIZE = 256

  # Solution A: keep 256 bytes for block metadata (header, lines),
  # which wastes around 113 bytes per block.
  LINE_COUNT = 127

  # Solution B: consider blocks to have 128 lines and consider the first line
  # to always have a minimal offset to avoid losing bytes for each block:
  #    offset = 128 - 16     == 144 bytes (header)
  # available = 256 - offset == 112 bytes (~ half line)
  #
  # LINE_COUNT = 128
  # BLOCK_FIRST_LINE_OFFSET = LINE_COUNT + sizeof(Header)

  # large objects are >= 8KB
  LARGE_OBJECT_SIZE = 8182
end

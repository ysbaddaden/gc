require "./block"

module GC
  struct BlockList
    @first = Pointer(Block).null
    @last = Pointer(Block).null
    @size = 0

    def clear
      @first = Pointer(Block).null
      @last = Pointer(Block).null
      @size = 0
    end

    def empty?
      @first.null?
    end

    def <<(item : Block*) : Nil
      push(item)
    end

    def push(item : Block*) : Nil
      if empty?
        @first = item
      else
        @last.value.next = item
      end
      item.value.next = Pointer(Block).null
      @last = item
      @size += 1
    end

    def shift : Block*
      item = @first
      unless item.null?
        @first = item.value.next
        @size -= 1
      end
      item
    end

    def size
      @size
    end
  end
end

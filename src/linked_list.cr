module GC
  struct LinkedList(T)
    @first = Pointer(T).null
    @last = Pointer(T).null
    @size = 0

    def clear
      @first = Pointer(T).null
      @last = Pointer(T).null
      @size = 0
    end

    def each : Nil
      item = @first
      until item.null?
        yield item
        item = item.value.next
      end
    end

    def empty?
      @first.null?
    end

    def insert(item : T*, after : T*) : Nil
      item.value.next = after.value.next
      after.value.next = item
      @last = item if after == @last
      @size += 1
    end

    def <<(item : T*) : Nil
      push(item)
    end

    def push(item : T*) : Nil
      if empty?
        @first = item
      else
        @last.value.next = item
      end
      item.value.next = Pointer(T).null
      @last = item
      @size += 1
    end

    def shift : T*
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

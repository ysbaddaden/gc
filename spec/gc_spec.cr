{% if flag?(:gc_immix) %}
  require "../src/gc"
{% end %}
require "spec"

describe GC do
  describe ".malloc" do
    it "allocates small object" do
      pointer = GC.malloc(8).as(UInt8*)
      8.times { |i| pointer[i] = i.to_u8 }
      8.times { |i| pointer[i].should eq(i) }
    end

    it "allocates medium object" do
      pointer = GC.malloc(1020).as(Int32*)
      255.times { |i| pointer[i] = i }
      255.times { |i| pointer[i].should eq(i) }
    end

    it "allocates large object" do
      pointer = GC.malloc(16730).as(Int64*)
      2091.times { |i| pointer[i] = i.to_i64 }
      2091.times { |i| pointer[i].should eq(i) }
    end
  end

  describe ".malloc_atomic" do
    it "allocates small object" do
      pointer = GC.malloc_atomic(8).as(UInt8*)
      8.times { |i| pointer[i] = i.to_u8 }
      8.times { |i| pointer[i].should eq(i) }
    end

    it "allocates medium object" do
      pointer = GC.malloc_atomic(1020).as(Int32*)
      255.times { |i| pointer[i] = i }
      255.times { |i| pointer[i].should eq(i) }
    end

    it "allocates large object" do
      pointer = GC.malloc_atomic(16730).as(Int64*)
      2091.times { |i| pointer[i] = i.to_i64 }
      2091.times { |i| pointer[i].should eq(i) }
    end
  end

  describe ".realloc" do
    it "resizes small object to larger allocation" do
      pointer = GC.malloc(8).as(UInt8*)
      8.times { |i| pointer[i] = i.to_u8 }

      pointer = GC.realloc(pointer.as(Void*), 16).as(UInt8*)
      8.times { |i| pointer[i + 8] = (i + 8).to_u8 }
      16.times { |i| pointer[i].should eq(i) }
    end

    it "resizes small object to smaller allocation" do
      pointer = GC.malloc(8).as(UInt8*)
      8.times { |i| pointer[i] = i.to_u8 }

      pointer = GC.realloc(pointer.as(Void*), 4).as(UInt8*)
      4.times { |i| pointer[i].should eq(i) }
    end

    it "resizes large object to larger allocation" do
      pointer = GC.malloc(8192).as(Int32*)
      2048.times { |i| pointer[i] = i }

      pointer = GC.realloc(pointer.as(Void*), 10_000).as(Int32*)
      452.times { |i| pointer[i + 2048] = i + 2048 }
      2500.times { |i| pointer[i].should eq(i) }
    end

    it "resizes large object to smaller allocation" do
      pointer = GC.malloc(10_000).as(Int32*)
      2500.times { |i| pointer[i] = i }

      pointer = GC.realloc(pointer.as(Void*), 8192).as(Int32*)
      2048.times { |i| pointer[i].should eq(i) }
    end

    it "resizes small object to become a large object" do
      pointer = GC.malloc(256).as(Int32*)
      64.times { |i| pointer[i] = i }

      pointer = GC.realloc(pointer.as(Void*), 8192).as(Int32*)
      1984.times { |i| pointer[i + 64] = i + 64 }
      2048.times { |i| pointer[i].should eq(i) }
    end

    it "resizes large object to become a small object" do
      pointer = GC.malloc(8192).as(Int32*)
      2048.times { |i| pointer[i] = i }

      pointer = GC.realloc(pointer.as(Void*), 256).as(Int32*)
      64.times { |i| pointer[i].should eq(i) }
    end
  end

  describe ".free" do
    it "small object" do
      pointer = GC.malloc(16)
      GC.free(pointer)
    end

    it "medium object" do
      pointer = GC.malloc(260)
      GC.free(pointer)
    end

    it "large object" do
      pointer = GC.malloc(8192)
      GC.free(pointer)
    end
  end

  it ".collect" do
    GC.collect
  end
end

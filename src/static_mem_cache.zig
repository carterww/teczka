const std = @import("std");
const assert = std.debug.assert;
const AtomicOrder = std.builtin.AtomicOrder;

fn MemCacheSlot(comptime T: type) type {
    return extern union {
        buffer: T,
        free_next: ?*MemCacheSlot(T),
    };
}

pub const StaticMemCacheError = error{
    OutOfMemory,
    InvalidPointer,
};

// A simple "allocator" that manages an array of objects of the same type.
// It is in charge of allocating and deallocating single objects from a
// static buffer passed in at instantiation time.
// It maintains very little state and uses no extra memory to maintain a free
// list. This fact, along with operating on objects of the same size in a static
// buffer, make it very easy to reason about and ensure correctness in programs.
pub fn StaticMemCache(comptime T: type) type {
    // We use a linked list to keep track of free slots. If a slot cannot
    // hold a pointer, we cannot use a linked list.
    if (@sizeOf(T) < @sizeOf(MemCacheSlot(T))) {
        std.debug.panic(
            ("Cannot instantiate an instance of StaticMemCache with " ++
                "type {s} because its size if less than a pointer."),
            .{@typeName(T)},
        );
    }
    return struct {
        const Self = @This();
        const Slot = MemCacheSlot(T);
        buffer: []Slot,
        free_first: ?*Slot = null,
        buffer_start_inc: usize,
        buffer_end_exc: usize,

        pub fn init(self: *Self, buffer: []T) void {
            std.debug.assert(buffer.len != 0);
            // This is safe because we checked at comptime that
            // a pointer can fit in T.
            const buffer_cast_ptr: [*]Slot = @ptrCast(buffer.ptr);
            self.buffer = buffer_cast_ptr[0..buffer.len];

            const buffer_uptr: usize = @intFromPtr(buffer.ptr);
            self.buffer_start_inc = buffer_uptr;
            self.buffer_end_exc = buffer_uptr + (buffer.len * @sizeOf(T));

            // Build free slot linked list
            var slot_prev: ?*Slot = null;
            for (self.buffer) |*slot| {
                slot.free_next = slot_prev;
                slot_prev = slot;
            }
            self.free_first = slot_prev;
        }

        pub fn create(self: *Self) !*T {
            while (true) {
                const free_slot = self.free_first;
                if (self.free_first == null) {
                    return StaticMemCacheError.OutOfMemory;
                }
                if (self.free_first_swap(free_slot, free_slot.?.free_next) == null) {
                    return &free_slot.?.buffer;
                } else {
                    continue;
                }
            }
        }

        pub fn destroy(self: *Self, ptr: *T) !void {
            const uptr: usize = @intFromPtr(ptr);
            if (uptr < self.buffer_start_inc or
                uptr >= self.buffer_end_exc)
            {
                return StaticMemCacheError.InvalidPointer;
            }
            const slot_ptr: *Slot = @ptrFromInt(uptr);
            while (true) {
                const free_first = self.free_first;
                slot_ptr.free_next = free_first;
                if (self.free_first_swap(free_first, slot_ptr) == null) {
                    return;
                } else {
                    continue;
                }
            }
        }

        fn free_first_swap(self: *Self, old: ?*Slot, new: ?*Slot) ??*Slot {
            return @cmpxchgWeak(
                ?*Slot,
                &self.free_first,
                old,
                new,
                AtomicOrder.monotonic,
                AtomicOrder.monotonic,
            );
        }
    };
}

const expect = std.testing.expect;
const expectEqual = std.testing.expectEqual;

const TEST_CACHE_SIZE: usize = 30;

test "init" {
    const allocator = std.testing.allocator;
    const fake_static_buffer = try allocator.alloc(usize, TEST_CACHE_SIZE);
    defer allocator.free(fake_static_buffer);

    var static_usize_mem_cache: StaticMemCache(usize) = undefined;
    static_usize_mem_cache.init(fake_static_buffer);

    const mc = &static_usize_mem_cache;
    try expectEqual(@intFromPtr(mc.buffer.ptr), @intFromPtr(fake_static_buffer.ptr));
    var free = mc.free_first;
    var free_count: usize = 0;
    while (free) |f| {
        free_count += 1;
        free = f.free_next;
    }
    try expectEqual(TEST_CACHE_SIZE, free_count);
    try expectEqual(@intFromPtr(mc.buffer.ptr), mc.buffer_start_inc);
    const last = &fake_static_buffer[TEST_CACHE_SIZE - 1];
    try expect(@intFromPtr(last) < mc.buffer_end_exc);
    try expect((@intFromPtr(last) + @sizeOf(usize)) >= mc.buffer_end_exc);
}

test "create" {
    const allocator = std.testing.allocator;
    const fake_static_buffer = try allocator.alloc(usize, TEST_CACHE_SIZE);
    defer allocator.free(fake_static_buffer);

    var static_usize_mem_cache: StaticMemCache(usize) = undefined;
    static_usize_mem_cache.init(fake_static_buffer);

    const mc = &static_usize_mem_cache;
    for (0..TEST_CACHE_SIZE) |_| {
        const buf = try mc.create();
        buf.* = 5;
    }
    const err = mc.create();
    try expect(err == StaticMemCacheError.OutOfMemory);
}

test "destroy catch invalid" {
    const allocator = std.testing.allocator;
    const fake_static_buffer = try allocator.alloc(usize, TEST_CACHE_SIZE);
    defer allocator.free(fake_static_buffer);

    var static_usize_mem_cache: StaticMemCache(usize) = undefined;
    static_usize_mem_cache.init(fake_static_buffer);

    const mc = &static_usize_mem_cache;
    try mc.destroy(&fake_static_buffer[0]);
    try mc.destroy(&fake_static_buffer[TEST_CACHE_SIZE - 1]);
    const err = mc.destroy(@ptrFromInt(@intFromPtr(&fake_static_buffer[TEST_CACHE_SIZE - 1]) + @sizeOf(usize)));
    try expect(err == StaticMemCacheError.InvalidPointer);
}

test "destroy mark free" {
    const allocator = std.testing.allocator;
    const fake_static_buffer = try allocator.alloc(usize, TEST_CACHE_SIZE);
    defer allocator.free(fake_static_buffer);

    var static_usize_mem_cache: StaticMemCache(usize) = undefined;
    static_usize_mem_cache.init(fake_static_buffer);

    const mc = &static_usize_mem_cache;
    const ptr = try mc.create();
    var free = mc.free_first;
    var free_count: usize = 0;
    while (free) |f| {
        free_count += 1;
        free = f.free_next;
    }
    try expect(free_count == TEST_CACHE_SIZE - 1);
    try mc.destroy(ptr);
    free = mc.free_first;
    free_count = 0;
    while (free) |f| {
        free_count += 1;
        free = f.free_next;
    }
    try expect(free_count == TEST_CACHE_SIZE);
}

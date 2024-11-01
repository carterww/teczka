const std = @import("std");
const DWCAS = @import("./dwcas.zig");

pub const SharedPointerAcquireErr = error{
    NullPointer,
    CASFailed,
};

fn SharedPointer(comptime T: type) type {
    // This struct will be the subject of our dwcas operation.
    // We extern it to ensure ptr stays at lower word.
    // We align ptr to arch's required address alignment
    return extern struct {
        const Self = @This();
        ptr: ?*T align(DWCAS.dwcas_addr_align),
        ref_count: isize,

        // Here we attempt to atomically acquire a reference to the pointer at
        // SharedPointer. To do this, we need to increment the reference count
        // iff
        // 1. The pointer has not changed
        // 2. The reference count has not changed
        // To accomplish this, we use a DWCAS operation to increment the
        // reference count. Using a DWCAS allows us to "swap" the ptr
        // with itself if it hasn't changed.
        pub fn acquire(self: *Self) !*T {
            const ptr = self.ptr; // Should probably be atomic load
            if (ptr == null) {
                return SharedPointerAcquireErr.NullPointer;
            }
            const ref_count_old = self.ref_count;
            const ref_count_new = ref_count_old + 1;
            const dwcas_descriptor = DWCAS.DWCASDescriptor{
                .addr = self.addr_cast_dwcas_UNSAFE(),
                .old_lo = ptr,
                .old_hi = ref_count_old,
                .new_hi = ref_count_new,
                .new_lo = ptr,
            };
            const success = DWCAS.dwcas(&dwcas_descriptor);
            return if (success) ptr else SharedPointerAcquireErr.CASFailed;
        }

        // Don't EVER use the result of this for anything other than the
        // address in the dwcas descriptor.
        fn addr_cast_dwcas_UNSAFE(self: *const Self) *usize {
            const ptr_usize: *usize = @ptrCast(&self.ptr);
            return ptr_usize;
        }
    };
}

fn AVLNode(comptime T: type) type {
    return struct {
        const Self = @This();
        left: SharedPointer(T),
        right: SharedPointer(T),
        balance_factor: i2,
        element: T,
    };
}

pub fn AVLTree(
    comptime T: type,
    comptime compare_fn: fn (a: *const T, b: *const T) i64,
) type {
    return struct {
        const Self = @This();
        const Node = AVLNode(T);
        root: SharedPointer(Node),

        fn node_compare(a: *const T, b: *const T) i64 {
            return compare_fn(a, b);
        }
    };
}

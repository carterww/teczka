const std = @import("std");
const builtin = @import("builtin");
const config = @import("config");
const assert = std.debug.assert;
const CallingConvention = std.builtin.CallingConvention;
const native_arch = builtin.cpu.arch;

pub const DWCASDescriptor = extern struct {
    addr: *usize align(dwcas_addr_align),
    old_lo: usize,
    old_hi: usize,
    new_hi: usize,
    new_lo: usize,
};

// The addres we are DWCASing must be aligned on some architectures
// For example, x86_64 requires the address to be 128 bit aligned.
pub const dwcas_addr_align: usize = switch (native_arch) {
    .x86_64 => 16,
    .aarch64 => 8,
    else => std.debug.panic("Unsupported Arch", .{}),
};

// Double word Compare and Swap.
// This function is implemented with native hardware dwcas instruction(s)
// on aarch64 and x86_64.
// Requirements:
// 1. d.addr must be aligned to dwcas_addr_align.
// 2. d.addr must immediately point to the lower word. d.addr++ must
//    point to the higher word.
// This function returns true if the dword was swapped, false otherwise.
pub fn dwcas(d: *const DWCASDescriptor) bool {
    // I somehow got lucky that I can have asm functions
    // on different archs with same param order and return
    // value.
    return dwcas_impl(
        d.addr,
        d.old_lo,
        d.old_hi,
        d.new_hi,
        d.new_lo,
    ) == 0;
}

const dwcas_impl = switch (native_arch) {
    .x86_64 => dwcas_impl_x86_64,
    .aarch64 => blk: {
        // Use ldaxp if specified in build script
        if (config.aarch_ldaxp) {
            break :blk dwcas_impl_aarch64_ldaxp;
        } else {
            break :blk dwcas_impl_aarch64_casp;
        }
    },
    else => std.debug.panic("Unsupported Arch", .{}),
};

extern fn dwcas_impl_x86_64(
    addr: *usize,
    old_lo: usize,
    old_hi: usize,
    new_hi: usize,
    new_lo: usize,
) callconv(CallingConvention.SysV) usize;

// Only works with ARMv8.1 or later due to the CASP instruction.
// https://developer.arm.com/documentation/dui0801/g/A64-Data-Transfer-Instructions/CASPA--CASPAL--CASP--CASPL--CASPAL--CASP--CASPL
extern fn dwcas_impl_aarch64_casp(
    addr: *usize,
    old_lo: usize,
    old_hi: usize,
    new_hi: usize,
    new_lo: usize,
) callconv(CallingConvention.AAPCS) usize;

extern fn dwcas_impl_aarch64_ldaxp(
    addr: *usize,
    old_lo: usize,
    old_hi: usize,
    new_hi: usize,
    new_lo: usize,
) callconv(CallingConvention.AAPCS) usize;

const expect = std.testing.expect;

const OLD_LO: usize = 0x5005;
const OLD_HI: usize = 0x4004;
const NEW_LO: usize = 0x7007;
const NEW_HI: usize = 0x6006;

fn dwcas_test_helper(
    modify_lo: bool,
    modify_hi: bool,
) !void {
    var addr align(dwcas_addr_align) = [2]usize{ OLD_LO, OLD_HI };
    var d: DWCASDescriptor = undefined;
    d.addr = @ptrCast(&addr[0]);
    d.old_lo = if (modify_lo) OLD_LO + 1 else OLD_LO;
    d.old_hi = if (modify_hi) OLD_HI + 1 else OLD_HI;
    d.new_hi = NEW_HI;
    d.new_lo = NEW_LO;
    const modify_any = modify_lo or modify_hi;
    try expect(modify_any != dwcas(&d));
    try expect(modify_any != (addr[0] == NEW_LO));
    try expect(modify_any != (addr[1] == NEW_HI));
}

test "dwcas success" {
    try dwcas_test_helper(false, false);
}

test "dwcas fails: lower modified" {
    try dwcas_test_helper(true, false);
}

test "dwcas fails: higher modified" {
    try dwcas_test_helper(false, true);
}

test "dwcas fails: dword modified" {
    try dwcas_test_helper(true, true);
}

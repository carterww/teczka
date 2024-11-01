.section .text
.global dwcas_impl_x86_64

# Calling convention
# This function uses the System V calling convention.
# Inputs:
# rdi - pointer to the memory location (128-bit aligned)
# rsi - old_lo
# rdx - old_hi
# rcx - new_hi
# r8  - new_lo
# Returns:
# 0 - success
# 1 - failure

dwcas_impl_x86_64:
    movq %rsi, %rax        # Move old_lo into RAX
    movq %r8, %rbx         # Move new_lo into RBX
    lock cmpxchg16b (%rdi) # Compare memory at [rdi] with RDX:RAX, swap with RCX:RBX if equal
    jz success             # Jump if comparison was successful (ZF = 1)
    movq $1, %rax          # Set return value to 1 (failure)
    ret
success:
    xorq %rax, %rax        # Set return value to 0 (success)
    ret


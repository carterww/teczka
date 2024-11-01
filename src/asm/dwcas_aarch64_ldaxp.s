.section .text
.global dwcas_impl_aarch64_ldaxp 

# Inputs:
# %x0 - pointer to the 128-bit memory location
# %x1 - old_lo
# %x2 - old_hi
# %x3 - new_hi
# %x4 - new_lo
# Returns:
# 0 - success
# 1 - failure

dwcas_impl_aarch64_ldaxp:
   # Move old values into scratch registers to be compared later for
   # success or failure
    mov %x1, %x6
    mov %x2, %x5
retry:
    ldaxp %x9, %x10, [%x0]
    # Compare to see if operation failed or succeeded
    cmp %x5, %x9
    ccmp %x6, %x10, #0, eq
    # If zero flag not set, go to failure
    bne failure

    # Try to store new value
    stlxp %w11, %x3, %x4, [%x0]
    # Retry if someone else accessed memory
    cbnz %w11, retry

    mov #0, %x0
    ret
failure:
    mov #1, %x0
    ret


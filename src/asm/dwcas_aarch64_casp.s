.section .text
.global dwcas_impl_aarch64_casp 

# Inputs:
# %x0 - pointer to the 128-bit memory location
# %x1 - old_lo
# %x2 - old_hi
# %x3 - new_hi
# %x4 - new_lo
# Returns:
# 0 - success
# 1 - failure

dwcas_impl_aarch64_casp:
   # Move old values into scratch registers to be compared later for
   # success or failure
    mov %x1, %x6
    mov %x2, %x5
    # Compare %x5:%x6 with contents of [%x0].
    # If equal, put %x3:%x4 into [%x0].
    # If not equal, new [%x0] values are put into %x5 and %x6
    caspal %x5, %x6, %x3, %x4, [%x0]
    # Compare to see if operation failed or succeeded
    cmp %x1, %x5
    ccmp %x2, %x6, #0, eq
    # If zero flag not set, go to failure
    bne failure

    mov #0, %x0
    ret
failure:
    mov #1, %x0
    ret


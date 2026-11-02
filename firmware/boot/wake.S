.section .wake.entry,"ax"
.global wake_entry
wake_entry:
    csrci mstatus, 8
    csrw mie, zero
    .option push
    .option norelax
    la gp, __global_pointer$
    .option pop
    la sp, _stack_top
    tail wake_main

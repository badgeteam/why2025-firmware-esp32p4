.section .data
.global _start
.type   _start,@function
_start:
sw a0, 0(a1)
ebreak

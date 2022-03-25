mov x0, 0x1
mov x1, 0x1
mov x0, 0x1
mov x1, 0x1
mov x0, 0x1
mov x1, 0x1
cmp x0, x1
beq t1

add x2, x0, x1
add x2, x0, x1
t1:
add x3, x0, x1
add x3, x0, x1
hlt 0

.text
mov x2, 0
mov x3, 100
mov x1, 0x1000
lsl X1, X1, 16
mov x2,0

foo:
add x2, x2, 1
cmp x3, x2
bgt foo
mov x4,0

hlt 0

.text
mov x2, 0
mov x3, 100
mov x1, 0x1000
lsl X1, X1, 16
mov x4, 3
mov x5, 4


foo:
add x2, x2, 1
stur x4, [x1, 0x0]
stur x5, [x1, 0x8]

cmp x3, x2
bgt foo

ldur x6, [x1, 0x0]
ldur x7, [x1, 0x8]
add x8, x6, x7
add x9, x8, x2
hlt 0



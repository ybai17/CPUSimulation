.text

mov x1, 0x1000
mov x4, 3
mov x5, 4
lsl x1, x1, 16
stur x4, [x1, 0x0]
ldur X6, [x1, 0x0]
stur x4, [x1, 0x20]
stur x5, [x1, 0x24]

hlt 0

file out/kernel.elf
target remote :1234
define rq
    disconnect
    target remote :1234
end
define cq
    target remote :1234
end

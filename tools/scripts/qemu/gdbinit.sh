set confirm off
set pagination off

# Enable pretty printing for structures
set print pretty on
set print array on
set print elements 0

# Show registers, source, and disassembly together
layout split
layout regs

# Stop on segfaults and errors
set breakpoint pending on
set print asm-demangle on
set disassembly-flavor intel

# Color (if your terminal supports it)
set style enabled on

# Symbol loading feedback
set verbose on

# History
set history save on
set history filename ~/.gdb_history
set history size 1000

# Easier inspection
define px
x/32x $arg0
end

define pxx
x/16xg $arg0
end

define pstr
x/s $arg0
end

define regs
info registers
end

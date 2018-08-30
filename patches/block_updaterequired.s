rel exe_text

# According to Luma, there's only one instance of the cart update pattern on NS <= 8.0.
# https://github.com/AuroraWright/Luma3DS/commit/f492318e3c15e16cf572c76cef06dd2014953f20

# Therefore, loop to replace these instances and exit upon not-found.

next:

find  0C18E1D8
jmpnf end
set   0B1821C8
jmp   next

end:

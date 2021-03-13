b=bytes()
assert(str(b) == 'bytes("")')
b=bytes("")
assert(str(b) == 'bytes("")')
b=bytes(0)
assert(str(b) == 'bytes("")')
b=bytes(1)
assert(str(b) == 'bytes("")')
b=bytes(-1)
assert(str(b) == 'bytes("")')

b=bytes("a")
assert(str(b) == 'bytes("")')
b=bytes(3.5)
assert(str(b) == 'bytes("")')
b=bytes([])
assert(str(b) == 'bytes("")')

b=bytes("1122AAaaBBbb")
assert(str(b) == 'bytes("1122AAAABBBB")')
b=bytes("112")
assert(str(b) == 'bytes("11")')
b=bytes("++")
assert(str(b) == 'bytes("00")')

b=bytes()
b.add(0x22)
assert(str(b) == 'bytes("22")')
b.add(0x12345678, 0)
assert(str(b) == 'bytes("22")')
b.add(0x12345678, 1)
assert(str(b) == 'bytes("2278")')
b.add(0x12345678, 2)
assert(str(b) == 'bytes("22787856")')
b.add(0x12345678, 4)
assert(str(b) == 'bytes("2278785678563412")')
b.add(0x12345678, -1) #- big endian -#
assert(str(b) == 'bytes("227878567856341278")')
b.add(0x12345678, -2)
assert(str(b) == 'bytes("2278785678563412785678")')
b.add(0x12345678, -4)
assert(str(b) == 'bytes("227878567856341278567812345678")')

b=bytes("000102030405")
assert(b.get(0) == 0)
assert(b.get(-1) == 0)  #- could consider nil as well -#
assert(b.get(6) == 0)  #- could consider nil as well -#
assert(b.get(1) == 1)
assert(b.get(5) == 5)

assert(b.get(1,0) == nil)
assert(b.get(1,1) == 0x01)
assert(b.get(1,2) == 0x0201)
assert(b.get(1,4) == 0x04030201)
assert(b.get(1,-1) == 0x01)
assert(b.get(1,-2) == 0x0102) #- big endian -#
assert(b.get(1,-4) == 0x01020304)

assert(bytes().size() == 0)
b=bytes("112233")
b.resize(2)
assert(str(b) == 'bytes("1122")')
assert(b.size() == 2)
b.resize(4)
assert(str(b) == 'bytes("11220000")')
assert(b.size() == 4)
b.resize(20)
assert(str(b) == 'bytes("1122000000000000000000000000000000000000")')
assert(b.size() == 20)
b.resize(0)
assert(str(b) == 'bytes("")')
assert(b.size() == 0)

b=bytes("aabb")
b.clear()
assert(str(b) == 'bytes("")')

assert(bytes() == bytes())
assert(bytes("11") == bytes("11"))
assert(bytes("11") == bytes()..0x11)
assert(! (bytes("11") == bytes(0x12)) )
assert(! (bytes("11") == 0x11) )
assert(! (bytes("11") != bytes("11")) )
assert(bytes("11") != bytes("1122"))
assert(bytes("11") != bytes("12"))
assert(bytes("11") != bytes())

b1 = bytes("1122")
b2 = bytes("334455")
b = b1 + b2
assert(str(b1) == 'bytes("1122")')
assert(str(b2) == 'bytes("334455")')
assert(str(b) == 'bytes("1122334455")')
b = b2 + b1
assert(str(b) == 'bytes("3344551122")')

b1 = bytes("1122")
b2 = bytes("334455")
b = b1..b2
assert(str(b1) == 'bytes("1122334455")')
assert(str(b2) == 'bytes("334455")')
assert(str(b) == 'bytes("1122334455")')

b = bytes("334455")
assert(b[0] == 0x33)
assert(b[1] == 0x44)
assert(b[2] == 0x55)

b = bytes("00112233445566778899AABBCCDDEEFF")
assert(str(b[1..1]) =='bytes("11")')
assert(str(b[-11..1]) =='bytes("0011")')
assert(str(b[0..40]) =='bytes("00112233445566778899AABBCCDDEEFF")')
assert(str(b[1..0]) =='bytes("")')

b=bytes("112233")
b2=b.copy()
assert(str(b) =='bytes("112233")')
assert(str(b2) =='bytes("112233")')
b2.clear()
assert(str(b) =='bytes("112233")')
assert(str(b2) =='bytes("")')

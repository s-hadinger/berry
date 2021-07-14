class A
    const a = 1 
    def f() end 
    var b 
    const c=3
end

a = A()
assert(a.a == 1)
assert(a.c == 3)

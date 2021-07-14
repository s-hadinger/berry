def assert_attribute_error(f)
    try
        f()
        assert(false, 'unexpected execution flow')
    except .. as e, m
        assert(e == 'attribute_error')
    end
end

class A
    static a
    def init() self.b = 2 end
    def f() end 
    var b 
    static c
end

print(type(A.a))
assert(A.a == nil)
assert(A.c == nil)
# assert_attribute_error(/-> A.b)
assert_attribute_error(/-> A.d)

a = A()
assert(a.b == 2)
assert(a.a == nil)
assert(a.c == nil)

A.a = 1
A.c = 3
assert(a.a == 1)
assert(a.c == 3)
assert(A.a == 1)
assert(A.c == 3)

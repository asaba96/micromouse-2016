a = 1
b = 1
c = -0.4
d = -1.6
func(x) = a * (x + b) ** (c) + d
fit func(x) 'left.cut' via a, b, c, d
plot func(x), 'left.cut'

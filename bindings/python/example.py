import displaz
from numpy.random import randn, rand

N = 100000
displaz.plot(40*randn(N, 3), color=rand(N,3)*[1,0.2,0.8])

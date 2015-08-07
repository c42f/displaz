using Displaz

function example(N)
    t = linspace(0,1, N)
    P = (10.+0.2*rand(size(t))).*[exp(5*t).*cos(100*pi*t)  exp(5*t).*sin(100*pi*t)  100*t];
    C = [t (1 .- t) zeros(t)];
    sz  = map(Float32, t)
    shape = mod(rand(Uint8, N), 6)
    clf()
    hold(true)
    plot(P; color = C)
    P[:,3] = -P[:,3]
    plot(P; color = C, markersize = sz, markershape = shape)
end

example(1_000_000)


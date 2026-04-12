# Zaryana

Z = (1 - distortion)
    · (1 - K_ZARYANA_CORR · |corruption - 0.5|)

ψ(x) = sign(x) · |x|^K_ZARYANA_POWER

axes = lerp(x, ψ(x), Z)
meta = lerp(m, m^K_ZARYANA_POWER, Z)
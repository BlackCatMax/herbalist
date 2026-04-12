# Morok

λ_raw =
corruption · (1 - purity)
· (1 + pressure)
· (1 + corruption_memory)
· (1 - clarity)

λ = clamp(λ_raw, 0, 1)^K_MOROK_POWER

s = smoothstep(0.2, 0.8, λ)
w = s²(3 - 2s)

f(x) = x
     + λ·sign(x)(1 - |x|)
     + w·λ²(x - sign(x)·K_MOROK_SHIFT)

distortion = λ
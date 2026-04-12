# Conflict Resolution

avg = mean(axes)

stab_eff = stability · (1 - pressure)

axes = lerp(axes, avg, stab_eff)

t = clamp(stability / COLLAPSE_THRESHOLD, 0, 1)
axes *= t
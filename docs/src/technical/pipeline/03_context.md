# Context

k_axis =
(1 - toxicity)
· fertility
· (1 - 0.5·competition)
· (1 + 0.3·moisture)
· (1 + 0.3·sin(2π·time_of_day))

k_meta =
(1 - 0.3·toxicity)
· (1 - 0.4·competition)
· (1 - 0.5·|moisture - 0.5|)
· (1 - 0.3·disturbance)
· (1 + 0.2·cos(2π·season))

axes *= k_axis  
meta *= k_meta
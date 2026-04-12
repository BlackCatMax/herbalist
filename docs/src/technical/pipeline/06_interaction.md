# Interaction Rules

1. a *= (1 + K_RES_GAIN · resonance)
2. a *= (1 + K_SYNERGY_GAIN · resonance)
3. a *= (1 - corruption · sign(a))

align = avg(original_axes)
a *= (1 + resonance · align)

potency  -= (1 - stability) * 0.3
stability -= potency * 0.4
purity   -= corruption
corruption -= K_PURITY_TO_CORRUPTION * purity
resonance -= K_AXIS_DIVERGENCE * |body - nature|
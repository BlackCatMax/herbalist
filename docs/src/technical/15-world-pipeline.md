# 15. Полный пайплайн мира

## 15.1 Порядок выполнения

```python
def game_cycle(player, world, inputs, intent, water):
    # 1. Input Assembly
    params = assemble_input(inputs, world.entities, player.memory, pressure(inputs))
    
    # 2. Process System
    state = aggregate(params.axes, params.meta, player.last_distortion)
    delta = calculate_delta(state.axes, state.meta)
    state.axes *= context_axis(world)
    state.meta *= context_meta(world)
    apply_water(state, water)
    apply_sign_modifiers(state)
    apply_interaction_rules(state)
    lam = morok(state, player.memory, world)
    state = apply_morok(state, delta, lam)
    resolve_conflicts(state)
    apply_collapse(state)
    z = zaryana(state, intent, lam)
    state = apply_zaryana(state, z)
    state = final_clamp(state)
    
    result = Result(state.axes, state.meta, lam)
    
    # 3. World Evolution
    world.toxicity += 0.01 * (result.corruption - 0.5)
    world.fertility += 0.01 * (result.resonance - 0.5)
    world.disturbance += 0.005 * (result.distortion - 0.5)
    world.competition += 0.005 * (0.5 - result.purity)
    apply_intent_modifiers(world, result, intent)
    
    for p in ["toxicity", "fertility", "disturbance", "competition", "spirit_pressure"]:
        setattr(world, p, max(0.0, min(1.0, getattr(world, p))))
    
    # 4. Biome change
    new_biome = get_biome(world)
    if new_biome != world.biome:
        apply_biome_change(new_biome, world.location)
        world.biome = new_biome
    
    # 5. Entities update
    world.entities = update_entities(world.biome, world, season, time, player, world.entities)
    for e in world.entities:
        e.attitude = attitude(e.spec, world, player)
    
    # 6. Memory update
    player.last_distortion = result.distortion
    player.corruption_memory = lerp(player.corruption_memory, result.corruption, 0.2)
    player.clarity = lerp(player.clarity, result.purity, 0.2)
    if check_fragment(player, world, result):
        player.memory_fragments = min(player.memory_fragments + 1, 12)
        player.clarity = max(player.clarity, player.memory_fragments / 12.0)
    
    return result, world
15.2 Схема
text
INPUT → PROCESS → RESULT → WORLD EVOLUTION → BIOME CHANGE → ENTITY UPDATE → MEMORY UPDATE → (loop)
15.3 Инварианты
Детерминизм: одинаковый вход → одинаковый выход

Однонаправленность: нет обратных связей внутри цикла

Нормализация только в конце

Мир меняется только через World Evolution
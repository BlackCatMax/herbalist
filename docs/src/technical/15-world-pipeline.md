# 15. Полный пайплайн мира

## 15.1 Порядок выполнения

def game_cycle(player, world, inputs, intent, water, order_specified, ordered_ingredients):
    
	========== 1. INPUT LAYER ==========
	
    1.1 Alchemy System
    weights = calculate_weights(inputs, order_specified, ordered_ingredients)
    pressure = K_PRESSURE_BASE + K_PRESSURE_COUNT * (len(inputs) / N_max)
    
    1.2 Input Assembly
    params = assemble_input(
        resources=inputs,
        entities=world.entities,
        memory=player.memory,
        pressure=pressure
    )
    
    ========== 2. PROCESS SYSTEM (канонический порядок) ==========
    
    2.1 Sequential Aggregation (fold) — если задан порядок
    if order_specified:
        state = neutral_state()   # S0: axes=0, potency=0, purity=1, stability=1, resonance=0.5, corruption=0
        for ingredient in ordered_ingredients:
            
			**partial_process — Process без Morok**
            state = partial_process(state, ingredient, weight, params.context, params.modifiers)
    else:
	
        **коммутативная агрегация**
        state = aggregate_commutative(params.axes, params.meta)
    
    2.2 Δ Calculation
    delta = calculate_delta(state.axes, state.meta, player.last_distortion)
    
    2.3 Morok (один раз)
    lam = morok(state, player.memory, world)
    state = apply_morok(state, delta, lam)
    
    2.4 Conflict Resolution
    resolve_conflicts(state, pressure)
    
    2.5 Collapse
    apply_collapse(state)
    
    2.6 Zaryana
    z = zaryana(state, intent, lam)
    state = apply_zaryana(state, z)
    
    2.7 Final Clamp
    state = final_clamp(state)
    
    result = Result(state.axes, state.meta, lam)
    
    ========== 3. WORLD EVOLUTION ==========
	
    world.toxicity += 0.01 * (result.corruption - 0.5)
    world.fertility += 0.01 * (result.resonance - 0.5)
    world.disturbance += 0.005 * (result.distortion - 0.5)
    world.competition += 0.005 * (0.5 - result.purity)
    apply_intent_modifiers(world, result, intent)
    
    for p in ["toxicity", "fertility", "disturbance", "competition", "spirit_pressure"]:
        setattr(world, p, max(0.0, min(1.0, getattr(world, p))))
    
    ========== 4. BIOME CHANGE ==========
	
    new_biome = get_biome(world)
    if new_biome != world.biome:
        apply_biome_change(new_biome, world.location)
        world.biome = new_biome
    
    ========== 5. ENTITIES UPDATE ==========
	
    world.entities = update_entities(world.biome, world, season, time, player, world.entities)
    for e in world.entities:
        e.attitude = attitude(e.spec, world, player)
    
    ========== 6. MEMORY UPDATE ==========
	
    player.last_distortion = result.distortion
    player.corruption_memory = lerp(player.corruption_memory, result.corruption, 0.2)
    player.clarity = lerp(player.clarity, result.purity, 0.2)
    if check_fragment(player, world, result):
        player.memory_fragments = min(player.memory_fragments + 1, 12)
        player.clarity = max(player.clarity, player.memory_fragments / 12.0)
    
    return result, world
	
## 15.2 Схема

INPUT → ASSEMBLY → (fold / commutative) → Δ → MOROK → CONFLICT → COLLAPSE → ZARYANA → CLAMP → RESULT → WORLD EVOLUTION → BIOME CHANGE → ENTITY UPDATE → MEMORY UPDATE → (loop)

## 15.3 Инварианты

- Детерминизм: одинаковый вход → одинаковый выход
- Однонаправленность: нет обратных связей внутри цикла
- Нормализация только в конце (Final Clamp)
- Мир меняется только через World Evolution
- Morok применяется один раз за вызов
- Контекст не изменяется внутри fold
- partial_process не содержит нелинейного искажения


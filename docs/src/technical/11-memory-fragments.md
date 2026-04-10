# 11. Фрагменты памяти (Memory Fragments)

## 11.1 Структура данных

```python
class MemorySystem:
    def __init__(self):
        self.last_distortion = 0.0
        self.corruption_memory = 0.0
        self.clarity = 1.0
        self.respect_score = 0.0
        self.memory_fragments = 0
11.2 Получение фрагментов
Фрагмент памяти добавляется при выполнении любого из условий:

python
def check_fragment_conditions(player, world_state, result, action, story_flag):
    if len(player.encountered_entities) % 3 == 0:
        return True
    if action == "cleanse_shrine" and world_state.spirit_pressure > 0.7:
        return True
    if result.intent == "ritual" and result.purity > 0.8:
        return True
    if story_flag in ["found_wolhv", "reached_buyan", "spoke_to_zaryana"]:
        return True
    return False
11.3 Обновление памяти
python
def update_memory(player, result, world_state, action, story_flag):
    player.last_distortion = result.distortion
    player.corruption_memory = lerp(player.corruption_memory, result.corruption, 0.2)
    player.clarity = lerp(player.clarity, result.purity, 0.2)
    
    if check_fragment_conditions(player, world_state, result, action, story_flag):
        player.memory_fragments = min(player.memory_fragments + 1, 12)
    
    fragment_bonus = player.memory_fragments / 12.0
    player.clarity = max(player.clarity, fragment_bonus)
11.4 Пороги концовок
python
ENDINGS = {
    "sacrifice": {"min": 0, "max": 12},
    "contract": {"min": 4, "max": 12},
    "illusion": {"min": 8, "max": 12},
    "knowledge": {"min": 12, "max": 12},
}

def get_available_endings(fragments):
    available = []
    for ending, thresholds in ENDINGS.items():
        if thresholds["min"] <= fragments <= thresholds["max"]:
            available.append(ending)
    return available
11.5 Влияние на Perception
python
def apply_fragment_effect(value, fragments):
    fragment_factor = fragments / 12.0
    clarity_effective = 0.5 + 0.5 * fragment_factor
    noise = (1 - clarity_effective) * 0.3
    return value + deterministic_noise(noise)
11.6 Конфигурационные константы
Константа	Значение
MAX_MEMORY_FRAGMENTS	12
FRAGMENT_CLARITY_BONUS_MAX	0.5
FRAGMENT_DISPLAY_NOISE_MAX	0.3
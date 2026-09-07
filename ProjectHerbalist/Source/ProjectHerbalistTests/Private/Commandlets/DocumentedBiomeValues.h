// DocumentedBiomeValues.h
//
// Числа биомов ровно так, как их задаёт КОМПЕНДИУМ
// (`herbalist_docs/Herbalist_Vault/04_Compendium/Биомы/*.md`). Почти все --
// построчно из frontmatter; `StressRecoveryMultiplier` -- выведенное самим
// компендиумом через `extract_biomes.py` (см. отдельный довод ниже). Ничего
// не додумано и не «уточнено» мной.
//
// Один общий заголовок на двух потребителей — коммандлет
// `-run=BiomeDefaultsSync`, который приводит к этим числам ассет, и тест
// `Herbalist.BiomeDefaults.AssetMatchesTheCompendium`, который стережёт,
// что ассет от них не уехал снова. Держать две копии одной таблицы значило
// бы, что тест однажды начнёт стеречь не то, что записал коммандлет.
//
// Почему числа зашиты в C++, а не читаются из .md в рантайме: карточки —
// человеческий Markdown с YAML-шапкой, парсить его в игровом модуле ради
// восьми строк дороже и хрупче, чем перенести значения один раз и накрыть
// тестом. Сверка при этом остаётся механической: при правке карточки тест
// упадёт и назовёт биом, поле и оба числа.
//
// ВНИМАНИЕ: список полей — ровно те, что компендиум ЗАДАЁТ, прямо или
// через свой собственный вывод.
//
// `StressRecoveryMultiplier` ДОБАВЛЕН 2026-09-07 вторым заходом, и это
// исправление моей же ошибки. В первом заходе я исключил его с доводом
// «документация не задаёт» — неверно: он ВЫВОДИТСЯ из компендиума
// скриптом `extract_biomes.py` (Fertility × (1 − Distortion×0.7),
// поправка на характер воды, нормировка на средний биом), и выведенные
// значения лежат в `CSV_tabs/DT_BiomeDefaults.json`. Проверено прогоном
// самого скрипта: он воспроизводит их из карточек и сообщает
// «расхождений нет». Цена ошибки была не нулевая — в ассете поле стояло
// нейтральной единицей у ВСЕХ восьми биомов, то есть механика «место
// держит след дольше или меньше» в игре просто не работала: Болото должно
// зарастать 12 игровых суток вместо 7, Смешанный лес — 4.1.
//
// По-прежнему НЕ входят `EntityActivityBase` и `DefaultWaterState`: у
// биомных карточек поле `water: []` пустое, а `EntityActivityBase`
// компендиум не задаёт ни прямо, ни выводом. Записывать значение, которого
// у документации нет, значило бы выдумать данные.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"

struct FDocumentedBiome
{
    EBiomeType Biome;
    const TCHAR* CardPath;   // откуда взято, для сообщений об ошибке
    float Body, Mind, Spirit, Nature;
    float Magnitude;
    float Distortion, Stability, Purity, Potency, Resonance, Corruption;
    float Toxicity, Fertility, Moisture;
    // Выведенное значение, не прямое из фронтматтера -- см. довод в шапке.
    float StressRecoveryMultiplier;
    const TCHAR* DisplayName;
};

namespace HerbalistDocumentedBiomes
{
    inline const TArray<FDocumentedBiome>& Get()
    {
        static const TArray<FDocumentedBiome> Values = {
            { EBiomeType::Tundra, TEXT("04_Compendium/Биомы/Тундра.md"),
              /*Direction*/ 0.40f, 0.40f, 0.60f, 0.60f,
              /*Magnitude*/ 0.45f,
              /*Meta D,S,P,Pot,Res,Cor*/ 0.30f, 0.50f, 0.70f, 0.55f, 0.55f, 0.20f,
              /*Env Tox,Fert,Moist*/ 0.20f, 0.30f, 0.40f,
              /*StressRecovery*/ 1.642f, TEXT("Тундра") },

            { EBiomeType::Taiga, TEXT("04_Compendium/Биомы/Тайга.md"),
              0.50f, 0.50f, 0.50f, 0.50f,
              0.60f,
              0.25f, 0.70f, 0.80f, 0.55f, 0.50f, 0.18f,
              0.15f, 0.60f, 0.60f,
              /*StressRecovery*/ 0.668f, TEXT("Тайга") },

            { EBiomeType::MixedForest, TEXT("04_Compendium/Биомы/Смешанный лес.md"),
              0.50f, 0.55f, 0.45f, 0.48f,
              0.65f,
              0.28f, 0.65f, 0.75f, 0.58f, 0.55f, 0.22f,
              0.25f, 0.70f, 0.60f,
              /*StressRecovery*/ 0.588f, TEXT("Смешанный лес") },

            { EBiomeType::BroadleafForest, TEXT("04_Compendium/Биомы/Широколиственный лес.md"),
              0.52f, 0.55f, 0.42f, 0.48f,
              0.70f,
              0.30f, 0.60f, 0.70f, 0.65f, 0.58f, 0.28f,
              0.30f, 0.75f, 0.55f,
              /*StressRecovery*/ 0.657f, TEXT("Широколиственный лес") },

            { EBiomeType::ForestSteppe, TEXT("04_Compendium/Биомы/Лесостепь.md"),
              0.58f, 0.58f, 0.38f, 0.42f,
              0.55f,
              0.35f, 0.55f, 0.60f, 0.50f, 0.50f, 0.35f,
              0.35f, 0.55f, 0.40f,
              /*StressRecovery*/ 0.797f, TEXT("Лесостепь") },

            { EBiomeType::Steppe, TEXT("04_Compendium/Биомы/Степь.md"),
              0.65f, 0.60f, 0.30f, 0.35f,
              0.45f,
              0.38f, 0.50f, 0.50f, 0.40f, 0.45f, 0.42f,
              0.40f, 0.45f, 0.30f,
              /*StressRecovery*/ 1.296f, TEXT("Степь") },

            { EBiomeType::Floodplain, TEXT("04_Compendium/Биомы/Речная пойма.md"),
              0.45f, 0.45f, 0.55f, 0.55f,
              0.75f,
              0.50f, 0.45f, 0.55f, 0.70f, 0.70f, 0.45f,
              0.45f, 0.80f, 0.80f,
              /*StressRecovery*/ 0.636f, TEXT("Речная пойма") },

            { EBiomeType::Bog, TEXT("04_Compendium/Биомы/Болото.md"),
              0.40f, 0.40f, 0.60f, 0.60f,
              0.75f,
              0.70f, 0.35f, 0.35f, 0.68f, 0.75f, 0.70f,
              0.70f, 0.60f, 0.90f,
              /*StressRecovery*/ 1.717f, TEXT("Болото") },
        };
        return Values;
    }
}

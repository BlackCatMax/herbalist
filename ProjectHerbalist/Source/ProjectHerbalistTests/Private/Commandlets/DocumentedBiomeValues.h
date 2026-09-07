// DocumentedBiomeValues.h
//
// Числа биомов ровно так, как их задаёт КОМПЕНДИУМ
// (`herbalist_docs/Herbalist_Vault/04_Compendium/Биомы/*.md`, frontmatter).
// Перенесены построчно, ничего не выведено и не «уточнено».
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
// ВНИМАНИЕ: список полей — ровно те, что компендиум ЗАДАЁТ. Намеренно
// отсутствуют `EntityActivityBase`, `DefaultWaterState` и
// `StressRecoveryMultiplier`: у биомных карточек поля `water: []` пустые,
// а `StressRecoveryMultiplier` по собственному комментарию в `BiomeRow.h`
// выводится скриптом `extract_biomes.py` из Fertility/Distortion и
// характера воды, то есть имеет свой производный источник. Записывать
// значение, которого документация не даёт, значило бы выдумать данные.
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
              /*Env Tox,Fert,Moist*/ 0.20f, 0.30f, 0.40f },

            { EBiomeType::Taiga, TEXT("04_Compendium/Биомы/Тайга.md"),
              0.50f, 0.50f, 0.50f, 0.50f,
              0.60f,
              0.25f, 0.70f, 0.80f, 0.55f, 0.50f, 0.18f,
              0.15f, 0.60f, 0.60f },

            { EBiomeType::MixedForest, TEXT("04_Compendium/Биомы/Смешанный лес.md"),
              0.50f, 0.55f, 0.45f, 0.48f,
              0.65f,
              0.28f, 0.65f, 0.75f, 0.58f, 0.55f, 0.22f,
              0.25f, 0.70f, 0.60f },

            { EBiomeType::BroadleafForest, TEXT("04_Compendium/Биомы/Широколиственный лес.md"),
              0.52f, 0.55f, 0.42f, 0.48f,
              0.70f,
              0.30f, 0.60f, 0.70f, 0.65f, 0.58f, 0.28f,
              0.30f, 0.75f, 0.55f },

            { EBiomeType::ForestSteppe, TEXT("04_Compendium/Биомы/Лесостепь.md"),
              0.58f, 0.58f, 0.38f, 0.42f,
              0.55f,
              0.35f, 0.55f, 0.60f, 0.50f, 0.50f, 0.35f,
              0.35f, 0.55f, 0.40f },

            { EBiomeType::Steppe, TEXT("04_Compendium/Биомы/Степь.md"),
              0.65f, 0.60f, 0.30f, 0.35f,
              0.45f,
              0.38f, 0.50f, 0.50f, 0.40f, 0.45f, 0.42f,
              0.40f, 0.45f, 0.30f },

            { EBiomeType::Floodplain, TEXT("04_Compendium/Биомы/Речная пойма.md"),
              0.45f, 0.45f, 0.55f, 0.55f,
              0.75f,
              0.50f, 0.45f, 0.55f, 0.70f, 0.70f, 0.45f,
              0.45f, 0.80f, 0.80f },

            { EBiomeType::Bog, TEXT("04_Compendium/Биомы/Болото.md"),
              0.40f, 0.40f, 0.60f, 0.60f,
              0.75f,
              0.70f, 0.35f, 0.35f, 0.68f, 0.75f, 0.70f,
              0.70f, 0.60f, 0.90f },
        };
        return Values;
    }
}

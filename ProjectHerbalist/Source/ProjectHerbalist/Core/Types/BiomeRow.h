// BiomeRow.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "BiomeRow.generated.h"

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FBiomeRow : public FTableRowBase
{
    GENERATED_BODY()

public:
    // Отображаемое имя (для отладки/UI)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FText DisplayName;

    // === Параметры состояния S_real (из ГДД таблица 8.1.1) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FDirection Direction;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    float Magnitude = 0.5f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "State")
    FMeta Meta;

    // === Параметры среды (toxicity, fertility, moisture) ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Environment")
    FEnvironment Environment;

    // === Дополнительные параметры из ГДД ===
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    float EntityActivityBase = 0.3f;    // базовая активность сущностей

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome")
    FRealState DefaultWaterState;        // состояние воды по умолчанию в этом биоме

    // Во сколько раз это место зарастает дольше базового
    // UHerbalistSettings::StressRecoveryGameDays. >1 — держит след дольше
    // (болото: стоячая вода, Навь мешает заживать), <1 — быстрее (пойма:
    // живая проточная вода промывает). Выводится из компендиума скриптом
    // extract_biomes.py из Fertility/Distortion и характера воды (поле `type`).
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Biome", meta = (ClampMin = "0.05"))
    float StressRecoveryMultiplier = 1.0f;
};

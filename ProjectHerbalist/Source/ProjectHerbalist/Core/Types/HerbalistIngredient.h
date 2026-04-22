// HerbalistIngredient.h
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Core/Types/HerbalistCoreTypes.h"   // для FRealState, EBiomeType
#include "HerbalistIngredient.generated.h"

// Предварительное объявление масок (они теперь не нужны, но оставим)
enum class ESeasonMask : uint8;
enum class ETimeOfDayMask : uint8;

/**
 * Единый Data Asset для описания ингредиента.
 * Содержит все данные: UI, базовые алхимические параметры, условия спавна в мире.
 */
UCLASS(BlueprintType)
class PROJECTHERBALIST_API UHerbalistIngredient : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    // --- Идентификация ---
    // Уникальный идентификатор (например "Bagulnik", "Klyukva").
    // Используется как PrimaryAssetId и для ссылок из кода.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
    FName IngredientID;

    // --- UI ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI", meta = (MultiLine = true))
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "UI")
    TSoftObjectPtr<UTexture2D> Icon;

    // --- Базовые алхимические параметры (S_real) ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    FRealState BaseState;

    // --- Параметры спавна в мире ---
    // Редкость (чем больше число, тем чаще появляется)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    int32 RarityWeight = 1;

    // В каких биомах может появиться
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning")
    TArray<EBiomeType> AllowedBiomes;

    // Сезонность (битовая маска) – пока не используется
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (Bitmask, BitmaskEnum = "/Script/ProjectHerbalist.ESeasonMask"))
    int32 SeasonMask = 0;

    // Время суток (битовая маска) – пока не используется
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Spawning", meta = (Bitmask, BitmaskEnum = "/Script/ProjectHerbalist.ETimeOfDayMask"))
    int32 TimeOfDayMask = 0;

    // --- Стихийная принадлежность ---
    // "Вода", "Огонь", "Земля", "Воздух", "Тьма", "Свет" и т.д.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Stats")
    FName Element;

    // --- Особые флаги ---
    // Является ли этот ингредиент водой (используется в алхимии)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
    bool bIsWater = false;

    // --- Теги (из компендиума) для возможных будущих механик ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Ingredient")
    TArray<FName> Tags;

    // --- Переопределение PrimaryAssetId ---
    virtual FPrimaryAssetId GetPrimaryAssetId() const override
    {
        return FPrimaryAssetId(StaticClass()->GetFName(), IngredientID);
    }
};
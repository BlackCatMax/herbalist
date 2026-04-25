#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Core/Data/IngredientTableRow.h"
#include "AlchemyTypes.generated.h"

// --- Atom Origin ---
UENUM()
enum class EAtomOrigin : uint8
{
    Harvest         UMETA(DisplayName = "Harvest"),
    Decomposition   UMETA(DisplayName = "Decomposition"),
    Spawn           UMETA(DisplayName = "Spawn"),
    Unknown         UMETA(DisplayName = "Unknown")
};

// --- Contribution Vector ---
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FContributionVector
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float Potency = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float Stability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float Instability = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float Noise = 0.0f;
};

// --- Alchemy Atom ---
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FAlchemyAtom
{
    GENERATED_BODY()

    // ============================================================
    // СУЩЕСТВУЮЩИЕ ПОЛЯ (используются в Semantics и IntentResolver)
    // ============================================================

    /** Является ли атом водой (используется в AlchemySemanticResolver). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    bool bIsWater = false;

    /** Идентификатор источника (например, имя ингредиента). Используется в AlchemySemanticResolver. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FName SourceID;

    /** Алхимическое состояние атома (используется в IntentResolver, PhysicsPipeline). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FRealState State;

    // ============================================================
    // НОВЫЕ ПОЛЯ (контракт §6, Фаза 1)
    // ============================================================

    /** Уникальный идентификатор атома. Присваивается при создании. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FGuid AtomUID;

    /** Класс ингредиента из реестра (может быть Unknown — контракт §5). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    EIngredientClass Class = EIngredientClass::Unknown;

    /** Контекст происхождения: Harvest, Decomposition, Spawn, Unknown. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    EAtomOrigin OriginContext = EAtomOrigin::Unknown;

    /** Снимок Distortion из FMemoryState::AccumulatedDistortion в момент создания атома. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float DistortionAtCollection = 0.3f;

    /** Игровое время создания (для derived-вычислений: порча, вызревание). */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    float TimeOfCreation = 0.0f;

    /** Вклад атома в алхимический процесс. Вычисляется PhysicsPipeline. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Alchemy")
    FContributionVector ContributionVector;

    // --- Конструкторы ---

    FAlchemyAtom();

    /** Полный конструктор с алхимическим состоянием. */
    FAlchemyAtom(FName InSourceID, bool bInIsWater, const FRealState& InState,
                 EAtomOrigin InOrigin, float InDistortionAtCollection, float InTimeOfCreation);
};

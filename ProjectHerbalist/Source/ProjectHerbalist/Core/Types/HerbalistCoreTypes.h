// HerbalistCoreTypes.h
#pragma once

#include "CoreMinimal.h"
#include "Math/UnrealMathUtility.h"
#include "HerbalistCoreTypes.generated.h"

// ========== Enum'ы ==========
UENUM(BlueprintType)
enum class EBiomeType : uint8
{
    Tundra,
    Taiga,
    MixedForest,
    BroadleafForest,
    ForestSteppe,
    Steppe,
    Floodplain,
    Bog
};

UENUM(BlueprintType)
enum class EAlchemyOutcome : uint8
{
    Valid,
    BoiledWater,
    Ash,
    Catastrophe,
    // Ветка Bifurcation "Purification" (05_Systems.md) — раньше делила
    // значение Valid с обычной удачной варкой, из-за чего ничто ниже по
    // цепочке (UI, RecordFootprint) не могло отличить редкое драматичное
    // очищение от рядового успеха, вопреки прямому требованию ГДД.
    Purified
};

// ========== Базовые структуры ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FDirection
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Body = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Mind = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Spirit = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Nature = 0.f;

    void NormalizeSum()
    {
        Body = FMath::Max(0.0f, Body);
        Mind = FMath::Max(0.0f, Mind);
        Spirit = FMath::Max(0.0f, Spirit);
        Nature = FMath::Max(0.0f, Nature);
        float Sum = Body + Mind + Spirit + Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Body /= Sum;
            Mind /= Sum;
            Spirit /= Sum;
            Nature /= Sum;
        }
        else
        {
            Body = Mind = Spirit = Nature = 0.25f;
        }
    }
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMeta
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Distortion = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Stability = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Purity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Potency = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Resonance = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Corruption = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRealState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Magnitude = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection Direction;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMeta Meta;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FEnvironment
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Toxicity = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Fertility = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Moisture = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FMemoryState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float AccumulatedDistortion = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float StabilityMemory = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float HistoryPurity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float DistortionVelocity = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Memory")
    float TimeOfLastDistortionChange = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FIntent
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float Coherence = 0.f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FRngState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Seed = 12345;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FWorldState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Env;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FIntent Intent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState CurrentState;
};

// ========== Структура клетки ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FGridCell
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 X = 0, Y = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EBiomeType Biome = EBiomeType::MixedForest;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState State;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FRealState TargetState;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FEnvironment Environment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMemoryState Memory;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float HarvestStress = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsWater = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName WaterTypeID = NAME_None;

    // Проявленная сущность (см. 02_GDD/16_Entity_Manifestation.md) — чисто
    // презентационное поле (как HarvestStress/Memory), не часть Command/Delta
    // цикла: выставляется/снимается в AGridWorldManager::UpdateEntityManifestations,
    // тем же "внепайплайновым" каналом, что уже используют RegenerateCellParameters
    // и ApplyBiomeInfluences. Пусто = ничего не проявлено.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FName ManifestedEntityID = NAME_None;

    // Список акторов ресурсов в этой клетке (слабые указатели, чтобы не мешать сборщику)
    UPROPERTY()
    TArray<TWeakObjectPtr<class AHerbalistResourceActor>> ResourceActors;
};

// ========== Сущности-"хозяева" (Основной уровень, 16_Entity_Manifestation §16.3) ==========
// Привязанный к конкретной клетке аккумулятор благосклонности — тот же принцип,
// что Restoration у капищ в 15_Cycles_And_Shrines, но без полноценной системы
// капищ: минимальная версия для одной клетки-"обиталища".
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FEntityLandmark
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FName EntityID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    FIntPoint Cell = FIntPoint(-1, -1);

    // [-1, 1]: <0 — осквернено/разгневано, >0 — благосклонно. Растёт от бережного
    // сбора (высокая Purity, низкий HarvestStress клетки), падает от истощения.
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Herbalist|Entities")
    float Respect = 0.0f;
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FConditionModifier
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FDirection DeltaDirection;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaMagnitude = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaDistortion = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaStability = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPurity = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaPotency = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaResonance = 0.0f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) float DeltaCorruption = 0.0f;

    FConditionModifier()
    {
        DeltaDirection.Body = 0.0f;
        DeltaDirection.Mind = 0.0f;
        DeltaDirection.Spirit = 0.0f;
        DeltaDirection.Nature = 0.0f;
    }
};

struct PROJECTHERBALIST_API FAlatyr
{
    static const FRealState S0;
};

// ========== FInventoryItem ==========
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FInventoryItem
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName IngredientID = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FRealState State;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Count = 1;

    // Время создания (мировое время, используется для расчёта порчи)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float CreationTime = 0.0f;

    // Подвержен ли предмет порче (по умолчанию true для собранных ингредиентов)
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bSubjectToDecay = true;

    // Является ли водой — нужно Pipeline'у для правил варки (05_Systems.md:
    // обязательность воды, разбавление, штраф >80% воды), не требует обращения
    // к реестрам (UIngredientRegistrySubsystem/UWaterTypeRegistrySubsystem),
    // проставляется один раз при харвесте.
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bIsWater = false;

    bool IsEmpty() const { return IngredientID.IsNone() || Count <= 0; }
    void Clear() { IngredientID = NAME_None; State = FRealState(); Count = 0; CreationTime = 0.0f; bSubjectToDecay = true; }
    bool IsValid() const { return !IngredientID.IsNone() && Count > 0; }
};

// ========== L2 Vector Direction ==========
// ... (без изменений)
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FL2Direction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Body = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Mind = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Spirit = 0.0f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float Nature = 0.0f;

    void NormalizeL2(FRngState& Rng)
    {
        float LenSq = Body * Body + Mind * Mind + Spirit * Spirit + Nature * Nature;
        if (LenSq > KINDA_SMALL_NUMBER)
        {
            float InvLen = FMath::InvSqrt(LenSq);
            Body *= InvLen;
            Mind *= InvLen;
            Spirit *= InvLen;
            Nature *= InvLen;
        }
        else
        {
            auto Rand01 = [&Rng]() {
                Rng.Seed = (Rng.Seed * 196314165) + 907633515;
                return (Rng.Seed & 0x00FFFFFF) / float(0x01000000);
                };
            float x1, x2, x3, x4, s;
            do {
                x1 = Rand01() * 2.0f - 1.0f;
                x2 = Rand01() * 2.0f - 1.0f;
                x3 = Rand01() * 2.0f - 1.0f;
                x4 = Rand01() * 2.0f - 1.0f;
                s = x1 * x1 + x2 * x2 + x3 * x3 + x4 * x4;
            } while (s > 1.0f || s < KINDA_SMALL_NUMBER);
            float InvLen = FMath::InvSqrt(s);
            Body = x1 * InvLen;
            Mind = x2 * InvLen;
            Spirit = x3 * InvLen;
            Nature = x4 * InvLen;
        }
    }

    FDirection ToL1() const
    {
        FDirection Result;
        Result.Body = FMath::Max(0.0f, Body);
        Result.Mind = FMath::Max(0.0f, Mind);
        Result.Spirit = FMath::Max(0.0f, Spirit);
        Result.Nature = FMath::Max(0.0f, Nature);
        float Sum = Result.Body + Result.Mind + Result.Spirit + Result.Nature;
        if (Sum > KINDA_SMALL_NUMBER)
        {
            Result.Body /= Sum;
            Result.Mind /= Sum;
            Result.Spirit /= Sum;
            Result.Nature /= Sum;
        }
        else
        {
            Result.Body = Result.Mind = Result.Spirit = Result.Nature = 0.25f;
        }
        return Result;
    }

    float Length() const
    {
        return FMath::Sqrt(Body * Body + Mind * Mind + Spirit * Spirit + Nature * Nature);
    }
};

inline FL2Direction ToL2(const FDirection& L1, FRngState& Rng)
{
    FL2Direction Result;
    Result.Body = L1.Body;
    Result.Mind = L1.Mind;
    Result.Spirit = L1.Spirit;
    Result.Nature = L1.Nature;
    Result.NormalizeL2(Rng);
    return Result;
}
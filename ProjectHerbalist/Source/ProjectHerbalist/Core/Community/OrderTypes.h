// Core/Community/OrderTypes.h
//
// Заказы и слава травника (2026-09-19, 02_GDD/24_Orders_And_Repute.md).
// Люди оставляют у порога записку с заказом и задатком; кто приходит, решает
// Молва (три круга гостей, доли плавно по Молве, §24.3). Заказ -- область в
// осях зелья (§24.4): у зелья нет «типа», оно точка в S_real. Сверка -- по
// S_real отданного, а травник выбирал по S_perceived (§24.5). Исход --
// наутро: Молва (§24.6), плата, для лихих -- последствия в мире (§24.8) или,
// если травник обманул, риск мести (§24.7).
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Engine/DataTable.h"
#include "OrderTypes.generated.h"

// Круг гостей (§24.3).
UENUM(BlueprintType)
enum class EOrderCircle : uint8
{
    Villagers,   // селяне
    Warriors,    // ратные и ловчие люди
    Outlaws      // лихие люди
};

// Ось направления зелья (FDirection).
UENUM(BlueprintType)
enum class EOrderAxis : uint8
{
    Body,
    Mind,
    Spirit,
    Nature
};

// Попадание в заказ (§24.5).
UENUM(BlueprintType)
enum class EOrderMatch : uint8
{
    Miss,    // мимо
    Edge,    // сойдёт -- на краю области
    Exact    // точно -- в области с запасом
};

// Что исполненный тёмный заказ делает в мире (§24.8).
UENUM(BlueprintType)
enum class EOrderConsequence : uint8
{
    None,
    Poison,          // падёж, хворь: Corruption у деревни
    SleepTheft,      // кража у соседа: только слух
    Charm,           // присушка: Distortion у деревни
    CurseNeighbour,  // порча на соседа: Corruption на одной клетке
    Revenge          // вскрытый обман лихого (§24.7): кража из погреба или порча на пороге
};

UENUM(BlueprintType)
enum class EOrderState : uint8
{
    Open,        // записка лежит, зелье не отдано
    Delivered    // зелье отдано, исход -- наутро
};

USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FOrderPayment
{
    GENERATED_BODY()

    // Ряд DT_IngredientClass; пусто -- ничего.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ItemID = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 1;
};

// Карточка заказа, ряд DT_Orders (-run=OrdersCreate).
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FOrderDefinition : public FTableRowBase
{
    GENERATED_BODY()

    FOrderDefinition()
    {
        MetaMax.Distortion = MetaMax.Stability = MetaMax.Purity = MetaMax.Potency = MetaMax.Resonance = MetaMax.Corruption = 1.0f;
    }

    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ID;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SortOrder = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOrderCircle Circle = EOrderCircle::Villagers;

    // Записка словами просителя (§24.2).
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText NoteText;

    // Область (§24.4): оси, которые должны быть ведущими (одна -- «преобладает»,
    // две -- «X и Y» занимают две верхние строчки), и границы меты.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EOrderAxis> LeadingAxes;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMeta MetaMin;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FMeta MetaMax;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 DeadlineDays = 3;

    // Задаток -- при чтении записки; плата -- наутро после исполнения;
    // прибавка -- только за «точно».
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOrderPayment Deposit;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOrderPayment Payment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FOrderPayment ExactBonus;

    UPROPERTY(EditAnywhere, BlueprintReadWrite) EOrderConsequence Consequence = EOrderConsequence::None;
    // Слух в Травнике, когда последствие сработало.
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText ConsequenceRumor;
};

// Открытый или исполненный, ещё не решённый заказ. Сохраняется.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FActiveOrder
{
    GENERATED_BODY()

    // Номер для игрока (DeliverOrder/RefuseOrder), растёт с каждым заказом.
    UPROPERTY() int32 Number = 0;
    UPROPERTY() FName DefinitionID;
    UPROPERTY() EOrderState State = EOrderState::Open;
    UPROPERTY() bool bNoteRead = false;
    UPROPERTY() double DeadlineClock = 0.0;
    UPROPERTY() double ResolveClock = 0.0;
    // Настоящее состояние отданного зелья -- сверка по нему (§24.5).
    UPROPERTY() FRealState DeliveredState;
};

// Отложенное последствие исполненного тёмного заказа (§24.8). Сохраняется.
USTRUCT(BlueprintType)
struct PROJECTHERBALIST_API FPendingOrderConsequence
{
    GENERATED_BODY()

    UPROPERTY() FName DefinitionID;
    UPROPERTY() EOrderConsequence Kind = EOrderConsequence::None;
    UPROPERTY() double FireClock = 0.0;
};

namespace HerbalistOrders
{
    // Доли кругов по Молве (§24.3), нормированы к 1:
    // w_селяне = max(0, 0.2 + M), w_ратные = 0.5 × clamp(M + 0.5, 0, 1),
    // w_лихие = max(0, 0.2 − M).
    PROJECTHERBALIST_API void ComputeCircleWeights(float Molva, float& OutVillagers, float& OutWarriors, float& OutOutlaws);

    // Попадание состояния в область заказа. OutDistance -- насколько состояние
    // вне области, 0…1 (0 -- внутри); это же вероятность вскрытия обмана
    // лихого (§24.7). EdgeMargin -- запас, начиная с которого «точно».
    // Ведущая ось наравне с не-ведущей -- мимо (пустое или ровное направление
    // ни во что не попадает); Magnitude ниже MinMagnitude -- мимо (пустышка).
    PROJECTHERBALIST_API EOrderMatch EvaluateOrderMatch(const FOrderDefinition& Def, const FRealState& State,
        float EdgeMargin, float MinMagnitude, float& OutDistance);

    // Предмет, который можно отдать по заказу, -- только сваренное зелье.
    PROJECTHERBALIST_API bool IsDeliverable(const FInventoryItem& Item);

    // DT_Orders, лениво (тот же приём, что DT_MemoryFragments).
    PROJECTHERBALIST_API const TArray<FOrderDefinition>& GetAllOrderDefinitions();
    PROJECTHERBALIST_API const FOrderDefinition* FindOrderDefinition(FName ID);
}

// RitualTypes.h
//
// Ритуальная (пошаговая, осмысленная) варка — 2026-08-30, прямой запрос:
// "мне нужно, чтобы какие-то зелья не просто варились закидыванием всего
// подряд, а осмысленно. например, сперва надо сварить 2 ингредиента в
// болотной воде на закате, потом добавлять третий с росой на рассвете".
//
// Отличие от обычной (импровизированной) варки — AGridWorldManager::
// TryAdvanceRitual: ингредиенты добавляются НЕ разом, а ШАГАМИ, каждый шаг
// требует своих условий (число ингредиентов на этот раз + фаза суток + тип
// воды среди добавляемого). Между шагами реально проходит игровое время —
// нельзя выполнить рассветный шаг сразу после закатного, так же как нельзя
// подменить условие числом ингредиентов. Прогресс хранится по клетке котла
// (AGridWorldManager::ActiveRituals), переживает произвольное число тиков
// между шагами.
//
// Правильно завершённый ритуал варится тем же честным ComputeApplyResult
// (PipelineV2.cpp), что и обычная варка (не отдельная, разошедшаяся с
// основной формула) — но с FApplyCommand::bIsRitual=true, что отключает
// градации опасности по числу ингредиентов (см. ComputeApplyResult §8):
// котёл наказывает за проигнорированную сложность рецепта, не за
// укрощённую верным порядком/местом/временем.
//
// "Роса" из примера пользователя пока не заведена отдельным типом воды/
// ингредиента (ни один существующий harvestable её не использует, см.
// ingredients.json/water_types.json) — первый рецепт ниже приближает её
// к "рассвет + вода среди добавляемого", не изобретает новую сущность без
// согласования. Сознательно небольшой реестр (1 рецепт) — тот же принцип,
// что уже применён к MemoryFragmentDefinitions.h/AmbientEntityTypes.h:
// простой статический список, не DataTable-пайплайн, пока рецептов мало.
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "RitualTypes.generated.h"

USTRUCT(BlueprintType)
struct FRitualStepDefinition
{
    GENERATED_BODY()

    // Сколько НЕ-водных ингредиентов игрок обязан добавить именно на этом
    // шаге (не суммарно за весь ритуал, и не считая воду -- "2 ингредиента
    // в болотной воде" значит 2 травы, вода отдельное требование среды, см.
    // RequiredWaterTypeID ниже). Не по конкретным ID -- "любые 2", не
    // "именно эти два" -- проще для первого рецепта, ID-привязка не
    // исключена для будущих.
    UPROPERTY(EditAnywhere) int32 IngredientCount = 1;

    UPROPERTY(EditAnywhere) bool bRequiresDawn = false;
    UPROPERTY(EditAnywhere) bool bRequiresDusk = false;
    UPROPERTY(EditAnywhere) bool bRequiresNight = false;

    // Один из добавляемых на этом шаге предметов обязан быть водой именно
    // этого WaterTypeID (см. DT_WaterTypes/water_types.json, например
    // "BogWater"). NAME_None = вода не важна на этом шаге.
    UPROPERTY(EditAnywhere) FName RequiredWaterTypeID = NAME_None;
};

USTRUCT(BlueprintType)
struct FRitualRecipeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName RecipeID;
    UPROPERTY(EditAnywhere) TArray<FRitualStepDefinition> Steps;
};

// Прогресс одного ритуала на одной клетке котла -- живёт между шагами,
// сколько бы игрового времени ни прошло (нет таймаута намеренно: реальные
// закат/рассвет и так естественно разносят шаги на игровые часы, отдельный
// счётчик "не успел" был бы наказанием сверху уже существующего).
USTRUCT()
struct FActiveRitualState
{
    GENERATED_BODY()

    UPROPERTY() FName RecipeID = NAME_None;
    UPROPERTY() int32 CompletedSteps = 0;
    UPROPERTY() TArray<FInventoryItem> AccumulatedIngredients;
};

enum class ERitualStepResult : uint8
{
    NoMatch,      // ни один известный рецепт не подошёл под эти условия/число
    Progressed,   // подошёл, но это не последний шаг -- ритуал продолжается
    Completed     // это был последний шаг -- зелье сварено
};

inline TArray<FRitualRecipeDefinition> GetRitualRecipeDefinitions()
{
    TArray<FRitualRecipeDefinition> Defs;

    // "Заревая вода" (рабочее имя) -- первый рецепт, ровно пример
    // пользователя: 2 ингредиента в болотной воде на закате, третий на
    // рассвете. Полюс "Заревая"/"Непочатая" уже существует в
    // HerbalistNameUtils.cpp как особый эпитет Purified -- правильно
    // исполненный ритуал тематически метит именно туда (не гарантия
    // исхода формулой, только тематическое созвучие; сама формула честно
    // считает Harmony/Bifurcation как обычно, просто без штрафов за число).
    {
        FRitualRecipeDefinition Def;
        Def.RecipeID = FName(TEXT("ZarevayaVoda"));

        FRitualStepDefinition Step1;
        Step1.IngredientCount = 2;
        Step1.bRequiresDusk = true;
        Step1.RequiredWaterTypeID = FName(TEXT("BogWater"));
        Def.Steps.Add(Step1);

        FRitualStepDefinition Step2;
        Step2.IngredientCount = 1;
        Step2.bRequiresDawn = true;
        Def.Steps.Add(Step2);

        Defs.Add(Def);
    }

    return Defs;
}

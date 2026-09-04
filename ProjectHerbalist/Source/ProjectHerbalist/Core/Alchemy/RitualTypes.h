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

    // Ключ-ингредиент (рецептурный гейт между ярусами биомов, ROADMAP.md
    // "Котёл — прокачка/инструменты" -> этот путь выбран вместо апгрейда
    // слотов котла, см. довод там же) -- пусто (как у "ЗаревойВоды" выше)
    // значит "любые N ингредиентов", тот же смысл, что и раньше, полная
    // обратная совместимость. Непусто -- хотя бы один из НЕ-водных
    // предметов, добавленных именно на этом шаге, обязан иметь
    // IngredientID из этого списка (не ЛЮБЫЕ N штук, а конкретное
    // растение/гриб этого яруса биомов, см. TryAdvanceRitual).
    UPROPERTY(EditAnywhere) TArray<FName> RequiredIngredientIDs;
};

USTRUCT(BlueprintType)
struct FRitualRecipeDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere) FName RecipeID;
    UPROPERTY(EditAnywhere) TArray<FRitualStepDefinition> Steps;

    // Рецептурный гейт между ярусами биомов -- NAME_None (как у "ЗаревойВоды"
    // выше) значит честная варка через ComputeApplyResult, как и раньше.
    // Непусто -- ритуал не варит зелье вовсе: завершение выдаёт готовый
    // предмет с этим IngredientID (см. TryAdvanceRitual, "Completed"),
    // награда -- кристалл-оберег следующего яруса, не осмысленно "сваренная"
    // из одного ингредиента жидкость.
    UPROPERTY(EditAnywhere) FName GrantsIngredientID = NAME_None;
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

    // Три рецептурных гейта между ярусами биомов (ROADMAP.md "Котёл —
    // прокачка/инструменты, дальше не в этом проходе: рецептурный гейт
    // между ярусами" -- закрыто этим путём вместо апгрейда слотов котла:
    // потребность/находка нового кристалла, не запертая дверь по числу
    // ингредиентов). Каждый требует ровно 1 ключ-ингредиент своего яруса
    // и верный час, БЕЗ требования к типу воды (гейт держится на растении
    // и времени суток, не на воде) -- один шаг, честно завершается сразу.
    // Награда каждого -- GrantsIngredientID (см. TryAdvanceRitual), НЕ
    // варится ComputeApplyResult'ом.

    // Ярус 1 (Тундра/Степь) -> Ярус 2. Ключ -- Лазорик (ste_06, Тюльпан
    // степной) ИЛИ Студёный мак (tun_04, Полярный мак), оба -- эндемики
    // ровно этого яруса (ROADMAP.md "биомный дисбаланс", 2026-09-04:
    // единственные 6 карточек, оставленные однобиомными как настоящие
    // ботанические специалисты, включают эти две). Рассвет -- порог,
    // пробуждение: "Алатырь" (награда, min_04 компендиума) -- бел-горюч
    // камень острова Буяна, на котором в заговорах вспыхивает заря.
    {
        FRitualRecipeDefinition Def;
        Def.RecipeID = FName(TEXT("PorogRassveta"));

        FRitualStepDefinition Step;
        Step.IngredientCount = 1;
        Step.bRequiresDawn = true;
        Step.RequiredIngredientIDs = { FName(TEXT("ste_06")), FName(TEXT("tun_04")) };
        Def.Steps.Add(Step);

        Def.GrantsIngredientID = FName(TEXT("Алатырь"));
        Defs.Add(Def);
    }

    // Ярус 2 (Тайга/Лесостепь) -> Ярус 3. Ключ -- Аконит (tai_10),
    // растение глухой тайги. Закат -- грань дня и ночи, опушка леса:
    // "Синь-камень" (награда, min_05) -- пограничный священный валун на
    // опушке/берегу, цвет закатных сумерек.
    {
        FRitualRecipeDefinition Def;
        Def.RecipeID = FName(TEXT("ZakatnayaOpushka"));

        FRitualStepDefinition Step;
        Step.IngredientCount = 1;
        Step.bRequiresDusk = true;
        Step.RequiredIngredientIDs = { FName(TEXT("tai_10")) };
        Def.Steps.Add(Step);

        Def.GrantsIngredientID = FName(TEXT("Синь-камень"));
        Defs.Add(Def);
    }

    // Ярус 3 (Смешанный/Широколиственный лес) -> Ярус 4. Ключ -- Бледная
    // поганка (mix_10), самый смертоносный гриб компендиума. Ночь --
    // полночь, омут, глубина: "Гагат" (награда, min_06) -- чёрный камень,
    // рождённый в толще болотной топи (окаменевшая древесина), тот же
    // мотив глубины/тьмы.
    {
        FRitualRecipeDefinition Def;
        Def.RecipeID = FName(TEXT("PolunochnyOmut"));

        FRitualStepDefinition Step;
        Step.IngredientCount = 1;
        Step.bRequiresNight = true;
        Step.RequiredIngredientIDs = { FName(TEXT("mix_10")) };
        Def.Steps.Add(Step);

        Def.GrantsIngredientID = FName(TEXT("Гагат"));
        Defs.Add(Def);
    }

    return Defs;
}

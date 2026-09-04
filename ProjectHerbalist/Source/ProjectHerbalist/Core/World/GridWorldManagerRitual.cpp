// Core/World/GridWorldManagerRitual.cpp
//
// Ритуальная (пошаговая, осмысленная) варка — Core/Alchemy/RitualTypes.h,
// 2026-08-30, прямой запрос ("не просто закидыванием всего подряд, а
// осмысленно — сперва 2 ингредиента в болотной воде на закате, потом
// третий на рассвете"). Продвижение шага и хранение прогресса —
// внепайплайновое (тот же принцип, что уже у Травника/подношения капищу в
// GridWorldManagerTick.cpp), но завершение ритуала честно варит через тот
// же Simulation::ExecutePipeline, что и обычная варка — не отдельная,
// разошедшаяся формула.

#include "Core/World/GridWorldManager.h"
#include "Core/Alchemy/RitualTypes.h"
#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Private/PipelineV2.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace
{
    // Один из добавляемых сейчас предметов — вода нужного WaterTypeID?
    // RequiredWaterTypeID сверяется с IngredientID воды (см. ProcessHarvestCommand:
    // WaterItem.IngredientID = Cell->WaterTypeID.IsNone() ? "Water" : WaterTypeID —
    // собранная вода уже несёт свой тип этим же полем, второго не заводим).
    bool HasRequiredWater(const TArray<FInventoryItem>& Items, FName RequiredWaterTypeID)
    {
        if (RequiredWaterTypeID.IsNone()) return true;
        for (const FInventoryItem& Item : Items)
        {
            if (Item.bIsWater && Item.IngredientID == RequiredWaterTypeID) return true;
        }
        return false;
    }

    // Рецептурный гейт между ярусами биомов (RitualTypes.h,
    // FRitualStepDefinition::RequiredIngredientIDs) -- хотя бы один из
    // НЕ-водных предметов, добавленных именно на этом шаге, обязан быть
    // конкретным ключ-ингредиентом яруса, не любой травой. Пусто =
    // "любые N" (как у "ЗаревойВоды"), тот же смысл, что и раньше.
    bool HasRequiredIngredient(const TArray<FInventoryItem>& Items, const TArray<FName>& RequiredIngredientIDs)
    {
        if (RequiredIngredientIDs.Num() == 0) return true;
        for (const FInventoryItem& Item : Items)
        {
            if (!Item.bIsWater && RequiredIngredientIDs.Contains(Item.IngredientID)) return true;
        }
        return false;
    }
}

ERitualStepResult AGridWorldManager::TryAdvanceRitual(const FIntPoint& CauldronCell,
    const TArray<FInventoryItem>& NewIngredients, FRandomStream& Rng, FInventoryItem& OutPotion)
{
    FActiveRitualState* Active = ActiveRituals.Find(CauldronCell);

    for (const FRitualRecipeDefinition& Recipe : GetRitualRecipeDefinitions())
    {
        // Если ритуал на этой клетке уже идёт — годится только ЕГО рецепт,
        // не любой другой из реестра (нельзя на середине "Заревой воды"
        // молча переключиться на другой рецепт тем же добавлением).
        if (Active && Active->RecipeID != Recipe.RecipeID) continue;

        const int32 StepIndex = Active ? Active->CompletedSteps : 0;
        if (!Recipe.Steps.IsValidIndex(StepIndex)) continue;
        const FRitualStepDefinition& Step = Recipe.Steps[StepIndex];

        // IngredientCount считает НЕ-водные предметы -- "2 ингредиента в
        // болотной воде" значит 2 травы плюс вода как отдельная, неявная
        // среда, не третий "ингредиент" в счёте (находка при калибровке
        // тестов: раньше вода тоже входила в счёт, и накопленные ритуалом
        // не-водные ингредиенты никогда не доходили даже до 3, поэтому
        // градации риска не могли сработать ни с ритуалом, ни без него).
        int32 NonWaterCount = 0;
        for (const FInventoryItem& Item : NewIngredients)
        {
            if (!Item.bIsWater) ++NonWaterCount;
        }
        if (Step.IngredientCount != NonWaterCount) continue;
        if (Step.bRequiresDawn && !IsDawn()) continue;
        if (Step.bRequiresDusk && !IsDusk()) continue;
        if (Step.bRequiresNight && !IsNight()) continue;
        if (!HasRequiredWater(NewIngredients, Step.RequiredWaterTypeID)) continue;
        if (!HasRequiredIngredient(NewIngredients, Step.RequiredIngredientIDs)) continue;

        // Условия шага выполнены -- продвигаем.
        FActiveRitualState NewState = Active ? *Active : FActiveRitualState();
        NewState.RecipeID = Recipe.RecipeID;
        NewState.CompletedSteps = StepIndex + 1;
        NewState.AccumulatedIngredients.Append(NewIngredients);

        if (NewState.CompletedSteps >= Recipe.Steps.Num())
        {
            // Рецептурный гейт между ярусами биомов (RitualTypes.h::
            // FRitualRecipeDefinition::GrantsIngredientID) -- награда не
            // варится ComputeApplyResult'ом вовсе: один ключ-ингредиент
            // "сварить" осмысленно нельзя, сама ритуальная форма тут --
            // способ получить редкий предмет (кристалл следующего яруса),
            // не алхимия. НЕ идёт мимо честной варки как обход/эксплойт --
            // это единственный путь получить именно этот предмет, он в
            // принципе не craftable обычным Apply.
            if (Recipe.GrantsIngredientID != NAME_None)
            {
                ActiveRituals.Remove(CauldronCell);

                FInventoryItem Reward;
                Reward.IngredientID = Recipe.GrantsIngredientID;
                Reward.Count = 1;
                Reward.bIsWater = false;

                // Резолв State через IngredientRegistrySubsystem -- тот же
                // приём, что уже AHerbalistPlayerController::ActivateWard
                // использует для резолва кристалла по имени. Недоступен в
                // Editor-мире автотестов (GameInstanceSubsystem) -- без
                // реестра предмет всё равно возвращается (голый
                // FRealState()), тесты бьют по IngredientID, не по State
                // (тот же класс пробела, что уже у TradeWithCommunity/
                // PlantSeed, см. ROADMAP.md).
                if (UGameInstance* GameInstance = GetGameInstance())
                {
                    if (UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance->GetSubsystem<UIngredientRegistrySubsystem>())
                    {
                        if (const FIngredientTableRow* Row = IngredientSubsystem->GetRow(Recipe.GrantsIngredientID))
                        {
                            Reward.State = Row->BaseState;
                        }
                    }
                }

                OutPotion = Reward;
                UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Ritual] %s completed at (%d,%d): granted '%s'"),
                    *Recipe.RecipeID.ToString(), CauldronCell.X, CauldronCell.Y, *Recipe.GrantsIngredientID.ToString());
                return ERitualStepResult::Completed;
            }

            // Ритуал завершён -- варим по-честному тем же пайплайном, что и
            // обычная варка, с bIsRitual=true (см. ComputeApplyResult §8 —
            // градации опасности по числу ингредиентов не действуют:
            // сложность укрощена верным порядком/местом/временем, не
            // проигнорирована).
            FWorldSnapshot EmptyWorldSnap;
            FInventorySnapshot EmptyInvSnap;
            FBiomeSnapshot EmptyBiomeSnap;
            FCommandBatch Batch;
            FCommandEntry Entry;
            Entry.Primitive = ECommandPrimitive::Apply;
            Entry.Apply.Ingredients = NewState.AccumulatedIngredients;
            Entry.Apply.bIsCrafting = true;
            Entry.Apply.bIsRitual = true;
            // Камень-оберег (21_Journey_And_Artifacts.md §21.3, Tier 1 п.1.3,
            // 2026-09-02) -- та же проверка, что уже AGridWorldManager::
            // ApplyAlchemyResult делает для обычной варки (GridWorldManagerAlchemy.cpp),
            // здесь была пропущена: ритуальная варка идёт мимо того пути.
            Entry.Apply.bBifurcationCharmActive = HasUnspentBifurcationCharm();
            Batch.AddCommand(Entry);
            FStateDelta Delta = Simulation::ExecutePipeline(EmptyWorldSnap, EmptyInvSnap, EmptyBiomeSnap, Batch, Rng);

            ActiveRituals.Remove(CauldronCell);

            const FInventoryOperation* AddOp = nullptr;
            for (const FInventoryOperation& Op : Delta.InventoryOps)
            {
                if (Op.OpType == EInventoryOpType::Add) AddOp = &Op;
            }
            if (!AddOp)
            {
                // ComputeApplyResult ничего не создал (пустой список и т.п.) --
                // не должно происходить при валидном рецепте, но не крашим.
                UE_LOG(LogHerbalistAlchemy, Warning, TEXT("[Ritual] %s completed but produced no potion"), *Recipe.RecipeID.ToString());
                return ERitualStepResult::NoMatch;
            }
            OutPotion = AddOp->Ingredient;

            UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Ritual] %s completed at (%d,%d): Outcome=%d"),
                *Recipe.RecipeID.ToString(), CauldronCell.X, CauldronCell.Y, (int32)OutPotion.BrewOutcome);
            return ERitualStepResult::Completed;
        }
        else
        {
            ActiveRituals.Add(CauldronCell, NewState);
            UE_LOG(LogHerbalistAlchemy, Log, TEXT("[Ritual] %s advanced to step %d/%d at (%d,%d)"),
                *Recipe.RecipeID.ToString(), NewState.CompletedSteps, Recipe.Steps.Num(), CauldronCell.X, CauldronCell.Y);
            return ERitualStepResult::Progressed;
        }
    }

    return ERitualStepResult::NoMatch;
}

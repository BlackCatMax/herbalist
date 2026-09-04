// Core/World/GridWorldManagerAlchemy.cpp
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"

// ============================================================================
// ПРИМЕНЕНИЕ АЛХИМИИ (ЧЕРЕЗ КОМАНДЫ НОВОГО ПАЙПЛАЙНА)
// ============================================================================

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FInventoryItem>& Ingredients, const FIntent& Intent)
{
    if (Ingredients.Num() == 0) return;

    FCommandEntry Cmd;
    Cmd.Primitive            = ECommandPrimitive::Apply;
    Cmd.Apply.TargetCell     = FIntPoint(X, Y);
    Cmd.Apply.Ingredients    = Ingredients;
    Cmd.Apply.Intent         = Intent;
    // Камень-оберег (21_Journey_And_Artifacts.md §21.3) — резолвится здесь,
    // вне Pipeline, тем же принципом, что и остальные внепайплайновые
    // входы команды (bIsRitual и т.п.).
    Cmd.Apply.bBifurcationCharmActive = HasUnspentBifurcationCharm();
    // Полнолуние поднимает Морок при варке (15_Cycles_And_Shrines.md §15.3,
    // Tier 1 п.1.2) -- тот же принцип, что и заряд оберега выше: резолвится
    // здесь, вне Pipeline.
    Cmd.Apply.MoonPhase = GetMoonPhase();
    // Оберег BrewBoost (Громовая стрела, §2.4, 2026-09-04) -- тот же приём,
    // что и bBifurcationCharmActive выше: резолвится здесь, не в Pipeline.
    Cmd.Apply.bWardBrewBoostActive = IsWardBrewBoostActive();
    // Межбиомная варка (§2.4, прямой запрос пользователя, 2026-09-04) -- тот
    // же принцип "резолвится здесь, не в Pipeline", что и остальные модификаторы
    // выше: считаем число РАЗНЫХ FInventoryItem::SourceBiome среди не-водных
    // ингредиентов (вода не имеет "места сбора" в смысле этого бонуса --
    // разбавитель, не трава) и кладём готовое число в команду, Pipeline
    // (ProcessApplyCommand) только читает.
    {
        TSet<EBiomeType> DistinctBiomes;
        for (const FInventoryItem& Ing : Ingredients)
        {
            if (!Ing.bIsWater)
            {
                DistinctBiomes.Add(Ing.SourceBiome);
            }
        }
        Cmd.Apply.DistinctIngredientBiomeCount = FMath::Max(1, DistinctBiomes.Num());
    }
    // Тиражный оберег BrewBoost (награда ритуала перехода ярусов биомов,
    // 2026-09-04) -- тот же принцип "резолвится здесь, не в Pipeline", что
    // и bWardBrewBoostActive/DistinctIngredientBiomeCount выше. Котёл стоит
    // на месте (дом) -- "биом игрока" бессмысленен, поэтому смотрим
    // FInventoryItem::SourceBiome каждого не-водного ингредиента этой варки:
    // полная сила (1.0), если ХОТЯ БЫ ОДИН собран в домашнем биоме
    // тиражного кристалла, иначе ослабленная (TieredWardOutOfBiomeStrength).
    // 0.0, если тиражный BrewBoost не активирован вовсе -- см. довод у
    // FApplyCommand::TieredBrewBoostStrength, CommandTypes.h.
    {
        float Strength = 0.0f;
        if (bTieredBrewBoostActive)
        {
            bool bAnyIngredientAtHome = false;
            for (const FInventoryItem& Ing : Ingredients)
            {
                if (!Ing.bIsWater && TieredBrewBoostHomeBiomes.Contains(Ing.SourceBiome))
                {
                    bAnyIngredientAtHome = true;
                    break;
                }
            }
            const UHerbalistSettings* Settings = GetHerbalistSettings();
            const float OutOfBiomeStrength = Settings ? Settings->TieredWardOutOfBiomeStrength : 0.5f;
            Strength = bAnyIngredientAtHome ? 1.0f : OutOfBiomeStrength;
        }
        Cmd.Apply.TieredBrewBoostStrength = Strength;
    }
    QueueCommand(Cmd);

    UE_LOG(LogHerbalistAlchemy, Log, TEXT("Queued Apply command for cell (%d,%d) with %d ingredients"), X, Y, Ingredients.Num());
}

// ============================================================================
// ПЕРЕГРУЗКА ДЛЯ СЫРЫХ СОСТОЯНИЙ
// ============================================================================

void AGridWorldManager::ApplyAlchemyResult(int32 X, int32 Y, const TArray<FRealState>& Ingredients, const FIntent& Intent)
{
    TArray<FInventoryItem> Items;
    Items.Reserve(Ingredients.Num());
    for (const FRealState& State : Ingredients)
    {
        FInventoryItem Item;
        Item.IngredientID = NAME_None;
        Item.State        = State;
        Item.Count        = 1;
        Items.Add(MoveTemp(Item));
    }
    ApplyAlchemyResult(X, Y, Items, Intent);
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ
// ============================================================================

void AGridWorldManager::ApplyTest(int32 X, int32 Y)
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalistAlchemy, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    if (Inventory.Num() < 2)
    {
        UE_LOG(LogHerbalistAlchemy, Warning, TEXT("Need at least 2 resources in inventory"));
        return;
    }

    FInventoryItem Ingredient1 = Inventory[0];
    FInventoryItem Ingredient2 = Inventory[1];

    TArray<FInventoryItem> Ingredients = { Ingredient1, Ingredient2 };
    // Coherence считается Pipeline'ом из Ingredients (ComputeIntentCoherence), не отсюда.
    FIntent Intent;

    ApplyAlchemyResult(X, Y, Ingredients, Intent);
    UE_LOG(LogHerbalistAlchemy, Log, TEXT("Queued Apply command for cell (%d,%d) and two resources"), X, Y);
}

void AGridWorldManager::ApplyPotionToCell(int32 X, int32 Y, const FRealState& PotionState)
{
    FInventoryItem PotionItem;
    PotionItem.IngredientID = FName(TEXT("Potion"));
    PotionItem.State        = PotionState;
    PotionItem.Count        = 1;

    TArray<FInventoryItem> Ingredients = { PotionItem };
    FIntent Intent;

    ApplyAlchemyResult(X, Y, Ingredients, Intent);
}
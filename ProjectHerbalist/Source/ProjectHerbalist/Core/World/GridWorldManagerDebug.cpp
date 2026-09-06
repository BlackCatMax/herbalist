// GridWorldManagerDebug.cpp
#include "Core/World/GridWorldManager.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Types/HerbalistCoreMath.h"
#include "Core/Journal/HerbalistJournalComponent.h"


void AGridWorldManager::SelectCell(int32 X, int32 Y)
{
    if (GetCell(X, Y))
    {
        SelectedX = X;
        SelectedY = Y;
        UE_LOG(LogHerbalistWorld, Log, TEXT("Selected cell (%d, %d)"), X, Y);
    }
    else
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("Invalid cell (%d, %d)"), X, Y);
    }
}

FString AGridWorldManager::GetSelectedCellInfo() const
{
    const FGridCell* Cell = GetCellConst(SelectedX, SelectedY);
    if (!Cell) return TEXT("No cell selected");
    // Вода не даёт ResourceActor -- клетка с bIsWater=true раньше печаталась
    // как "Resource=None", неотличимо от обычной пустой суши (2026-09-03,
    // найдено при проверке 0.5 плана: SelectCell/Info -- единственный
    // консольный способ проверить размещение Water Region Volume, и он был
    // слеп именно к воде). GetSelectedCellInfoBP (BP-путь) уже отличал воду
    // литералом "Вода" -- здесь печатаем настоящий WaterTypeID, эта строка
    // для отладки в консоли, не для игрока.
    FString ResourceStr = TEXT("None");
    if (Cell->bIsWater)
    {
        ResourceStr = FString::Printf(TEXT("Water(%s)"), *Cell->WaterTypeID.ToString());
    }
    else if (Cell->ResourceActors.Num() > 0 && Cell->ResourceActors[0].IsValid())
    {
        ResourceStr = Cell->ResourceActors[0]->GetIngredientID().ToString();
    }
    // Счётчик (2026-09-04, "почему некоторые квадраты полностью пустые...").
    // Раньше строка показывала только ПЕРВЫЙ ресурс клетки -- "x123" от "x1"
    // было неотличимо без открытия аутлайнера. Только у непустых (Num()==0
    // не добавляет суффикс -- "None"/"Water(...)" остаются как есть, ровно
    // тем текстом, что уже проверяет SelectedCellInfoTest.cpp). Ресурсы на
    // воде (аквапул, тоже лежат в ResourceActors) считаются тем же полем --
    // "Water(bol_wt) x2" читается однозначно.
    if (Cell->ResourceActors.Num() > 0)
    {
        ResourceStr += FString::Printf(TEXT(" x%d"), Cell->ResourceActors.Num());
    }
    // Distance(S_real, S0) — раньше был написан и не подключён ни к чему
    // (META_AUDIT §1.4). Первый живой потребитель: отладочная строка
    // выделенной клетки. Не нормализовано в [0,1] — см. HerbalistCoreMath.h.
    // Метрика мира с историей (15_Cycles_And_Shrines.md §15.5.1, 2026-09-06) —
    // тот же Distance_итог, что и у условия Буяна (CheckBuyanCondition),
    // не голый снимок State.
    const float DistanceToS0 = HerbalistCore::Math::DistanceWithHistory(Cell->State, Cell->Memory.AverageCoherence);

    // Курган (§2.3/§4.3, 2026-09-06) — единственный способ обнаружить место
    // без визуального актора на уровне (v1 консольный, тот же принцип, что
    // остальные Exec-первопроходы этого проекта): непустая строка означает
    // ещё не разграбленный курган на выделенной клетке (LootKurgan снимает
    // запись из KurganSites после выдачи, суффикс исчезает сам собой).
    FString KurganStr;
    if (const FName* Loot = KurganSites.Find(FIntPoint(SelectedX, SelectedY)))
    {
        KurganStr = FString::Printf(TEXT(", Kurgan=%s"), *Loot->ToString());
    }

    return FString::Printf(TEXT("Cell (%d,%d): Mag=%.2f, Dist=%.2f, Stress=%.3f, DistToS0=%.2f, Resource=%s%s"),
        SelectedX, SelectedY,
        Cell->State.Magnitude,
        Cell->State.Meta.Distortion,
        Cell->HarvestStress,
        DistanceToS0,
        *ResourceStr,
        *KurganStr);
}

void AGridWorldManager::GetSelectedCellInfoBP(int32& X, int32& Y, FString& ResourceName, float& RegrowthTimer, float& Distortion, float& HarvestStress)
{
    X = SelectedX;
    Y = SelectedY;
    RegrowthTimer = 0.0f;
    Distortion = 0.0f;
    HarvestStress = 0.0f;
    ResourceName = TEXT("None");
    if (X >= 0 && Y >= 0)
    {
        FGridCell* Cell = GetCell(X, Y);
        if (Cell)
        {
            Distortion = Cell->State.Meta.Distortion;
            HarvestStress = Cell->HarvestStress;
            if (Cell->bIsWater)
            {
                ResourceName = TEXT("Вода");
            }
            else if (Cell->ResourceActors.Num() > 0 && Cell->ResourceActors[0].IsValid())
            {
                ResourceName = Cell->ResourceActors[0]->GetIngredientID().ToString();
            }
        }
    }
}

void AGridWorldManager::ShowInventory()
{
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->InventoryComponent)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("No player controller or inventory component found"));
        return;
    }

    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;

    TArray<FInventoryItem> Inventory = PC->InventoryComponent->GetItems();
    UE_LOG(LogHerbalistWorld, Log, TEXT("=== INVENTORY (%d items) ==="), Inventory.Num());
    for (int32 i = 0; i < Inventory.Num(); ++i)
    {
        const FInventoryItem& Item = Inventory[i];
        const FRealState& Res = Item.State;
        FString Name;
        if (Item.IngredientID == FName(TEXT("Potion")))
        {
            Name = TEXT("Зелье");
        }
        else if (Item.IngredientID == FName(TEXT("Water")))
        {
            Name = TEXT("Вода");
        }
        else if (IngredientSubsystem)
        {
            if (const FIngredientTableRow* Row = IngredientSubsystem->GetRow(Item.IngredientID))
            {
                Name = Row->DisplayName.ToString();
            }
            else
            {
                Name = Item.IngredientID.ToString();
            }
        }
        else
        {
            Name = Item.IngredientID.ToString();
        }
        UE_LOG(LogHerbalistWorld, Log, TEXT("[%d] %s x%d: Mag=%.2f, Dist=%.2f, Pot=%.2f Res=%.2f Cor=%.2f, Dir: (%.2f,%.2f,%.2f,%.2f)"),
            i, *Name, Item.Count, Res.Magnitude, Res.Meta.Distortion, Res.Meta.Potency, Res.Meta.Resonance, Res.Meta.Corruption,
            Res.Direction.Body, Res.Direction.Mind, Res.Direction.Spirit, Res.Direction.Nature);
    }
}

void AGridWorldManager::ShowShrines()
{
    // Первый живой потребитель капищ (15_Cycles_And_Shrines §15.5) — до какого
    // бы то ни было UI проверяем сам факт роста/спада Restoration.
    UE_LOG(LogHerbalistWorld, Log, TEXT("=== SHRINES (%d) ==="), Shrines.Num());
    for (int32 i = 0; i < Shrines.Num(); ++i)
    {
        const FShrine& S = Shrines[i];
        UE_LOG(LogHerbalistWorld, Log, TEXT("[%d] (%d,%d) type=%d Restoration=%.3f"),
            i, S.Cell.X, S.Cell.Y, (int32)S.Type, S.Restoration);
    }
}

void AGridWorldManager::ShowJournal()
{
    // Первый живой потребитель Травника (07_UX §7.2.4) — до UI/подсветки
    // закономерностей проверяем сам факт записи. Печатает PerceivedState —
    // намеренно: журнал показывает игроку то же, что видел бы он сам,
    // не S_real (см. предупреждение в JournalTypes.h).
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->JournalComponent)
    {
        UE_LOG(LogHerbalistWorld, Warning, TEXT("No player controller or journal component found"));
        return;
    }

    const TArray<FJournalEntry>& Entries = PC->JournalComponent->GetEntries();
    UE_LOG(LogHerbalistWorld, Log, TEXT("=== TRAVNIK JOURNAL (%d entries) ==="), Entries.Num());
    for (int32 i = 0; i < Entries.Num(); ++i)
    {
        const FJournalEntry& E = Entries[i];
        const FRealState& S = E.PerceivedState;
        UE_LOG(LogHerbalistWorld, Log,
            TEXT("[%d] %s %s x%d @ (%d,%d) biome=%d night=%d t=%.1f | Mag=%.2f Dist=%.2f Pur=%.2f Cor=%.2f"),
            i, E.Type == EJournalEntryType::Harvest ? TEXT("Harvest") : TEXT("Brew"),
            *E.IngredientID.ToString(), E.Count, E.Cell.X, E.Cell.Y, (int32)E.Biome, E.bWasNight ? 1 : 0,
            E.GameTimeSeconds, S.Magnitude, S.Meta.Distortion, S.Meta.Purity, S.Meta.Corruption);
    }
}
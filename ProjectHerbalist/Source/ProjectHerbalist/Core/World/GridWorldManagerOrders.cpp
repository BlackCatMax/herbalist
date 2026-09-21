// Core/World/GridWorldManagerOrders.cpp
//
// Заказы и слава травника (02_GDD/24_Orders_And_Repute.md). Внепайплайновая
// часть менеджера, тот же приём, что фрагменты памяти: тик из
// AGridWorldManager::Tick (UpdateOrders), мировые акторы-записки, Молва --
// поле менеджера. Все числа -- UHerbalistSettings, категория "Orders".
//
// ЯВНОЕ ИСКЛЮЧЕНИЕ из Single-Writer, как SeedRosaCorruptedCircle
// (GridWorldManagerZaryana.cpp): последствия тёмных заказов и месть пишут
// State.Meta клеток у дома напрямую (AddStateAroundHome), мимо ApplyStateDelta,
// -- это след события, а не источник порчи, TargetState не трогается, клетки
// зарастают сами. В трассе реплея (TraceReplay) этих записей нет. Как и у
// фрагментов, броски -- из общего WorldRNG (одноразовые события рантайма).

#include "Core/World/GridWorldManager.h"
#include "Core/Community/OrderCacheActor.h"
#include "Core/Community/OrderNoteActor.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Engine/GameInstance.h"
#include "EngineUtils.h"
#include "Player/HerbalistPlayerController.h"
#include "HerbalistLogChannels.h"

namespace
{
    double OrderDayLengthSeconds(const UHerbalistSettings* Settings)
    {
        return FMath::Max(1.0f, Settings ? Settings->GameDayMinutes : 32.0f) * 60.0;
    }
}

double AGridWorldManager::GetOrderDayLengthSeconds() const
{
    return OrderDayLengthSeconds(GetHerbalistSettings());
}

FName AGridWorldManager::PickOrderDefinition(FRandomStream& Rng) const
{
    const TArray<FOrderDefinition>& All = HerbalistOrders::GetAllOrderDefinitions();
    if (All.Num() == 0)
    {
        return NAME_None;
    }

    // Круг -- по долям Молвы (§24.3), заказ -- равновероятно внутри круга.
    float Villagers = 0.0f;
    float Warriors = 0.0f;
    float Outlaws = 0.0f;
    HerbalistOrders::ComputeCircleWeights(Molva, Villagers, Warriors, Outlaws);
    const float Roll = Rng.FRand();
    const EOrderCircle Circle = Roll < Villagers ? EOrderCircle::Villagers
        : Roll < Villagers + Warriors ? EOrderCircle::Warriors : EOrderCircle::Outlaws;

    TArray<FName> Candidates;
    for (const FOrderDefinition& Def : All)
    {
        if (Def.Circle == Circle)
        {
            Candidates.Add(Def.ID);
        }
    }
    return Candidates.Num() > 0 ? Candidates[Rng.RandRange(0, Candidates.Num() - 1)] : NAME_None;
}

int32 AGridWorldManager::IssueOrder(FName DefinitionID)
{
    const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(DefinitionID);
    if (!Def)
    {
        return 0;
    }
    FActiveOrder& Order = ActiveOrders.AddDefaulted_GetRef();
    Order.Number = NextOrderNumber++;
    Order.DefinitionID = DefinitionID;
    Order.DeadlineClock = GameClockSeconds + FMath::Max(1, Def->DeadlineDays) * GetOrderDayLengthSeconds();
    SpawnOrderNote(Order);
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Записка %d: %s (срок %d сут.)"), Order.Number, *DefinitionID.ToString(), Def->DeadlineDays);
    return Order.Number;
}

void AGridWorldManager::SpawnOrderNote(const FActiveOrder& Order)
{
    // Записка -- у порога дома (клетка Заряны, она же дом с котлом). Дома ещё
    // нет (тест, пустая карта) -- заказ всё равно открыт, читается командой
    // ListOrders.
    if (Order.bNoteRead || !HerbalistCore::IsValidCell(ZaryanaCell) || !GetWorld())
    {
        return;
    }
    if (const TWeakObjectPtr<AOrderNoteActor>* Existing = OrderNoteActors.Find(Order.Number))
    {
        if (Existing->IsValid())
        {
            return;
        }
    }
    // Записки ложатся рядком у порога, не одна на другую: место -- номер
    // записки среди непрочитанных, чуть в стороне от центра клетки (там стол).
    constexpr float NoteSpacingCm = 60.0f;
    constexpr float DoorstepOffsetCm = 120.0f;
    int32 Slot = 0;
    for (const FActiveOrder& Other : ActiveOrders)
    {
        if (Other.Number < Order.Number && Other.State == EOrderState::Open && !Other.bNoteRead)
        {
            ++Slot;
        }
    }
    const FVector Offset(DoorstepOffsetCm + NoteSpacingCm * Slot, DoorstepOffsetCm, 10.0f);
    AOrderNoteActor* Note = GetWorld()->SpawnActor<AOrderNoteActor>(AOrderNoteActor::StaticClass(),
        GetCellWorldPosition(ZaryanaCell.X, ZaryanaCell.Y) + Offset, FRotator::ZeroRotator);
    if (Note)
    {
        Note->Init(Order.Number, this);
        OrderNoteActors.Add(Order.Number, Note);
    }
}

FActiveOrder* AGridWorldManager::FindActiveOrder(int32 Number)
{
    return ActiveOrders.FindByPredicate([Number](const FActiveOrder& Order) { return Order.Number == Number; });
}

void AGridWorldManager::ReadOrderNote(int32 Number, AHerbalistPlayerController* PC)
{
    FActiveOrder* Order = FindActiveOrder(Number);
    const FOrderDefinition* Def = Order ? HerbalistOrders::FindOrderDefinition(Order->DefinitionID) : nullptr;
    if (!Def || Order->bNoteRead)
    {
        return;
    }
    Order->bNoteRead = true;
    OrderNoteActors.Remove(Number);
    GiveOrderPayment(Def->Deposit);
    if (PC)
    {
        PC->ShowMemoryRevealText(Def->NoteText);
    }
    PostCommunityNote(FText::Format(NSLOCTEXT("Orders", "NoteEntry", "Заказ {0}: {1}"), FText::AsNumber(Number), Def->NoteText));
}

bool AGridWorldManager::DeliverOrder(int32 Number, const FInventoryItem& Potion)
{
    FActiveOrder* Order = FindActiveOrder(Number);
    if (!Order || Order->State != EOrderState::Open || !HerbalistOrders::IsDeliverable(Potion))
    {
        return false;
    }
    // Сверка -- по настоящему состоянию того, что отдано (§24.5), исход --
    // наутро: на рассвете следующих суток.
    const double DayLength = GetOrderDayLengthSeconds();
    Order->State = EOrderState::Delivered;
    Order->DeliveredState = Potion.State;
    Order->ResolveClock = (FMath::FloorToDouble(GameClockSeconds / DayLength) + 1.0) * DayLength;
    if (!Order->bNoteRead)
    {
        // Отдал, не читая записку, -- задаток всё равно забирает.
        if (const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(Order->DefinitionID))
        {
            Order->bNoteRead = true;
            GiveOrderPayment(Def->Deposit);
        }
        if (const TWeakObjectPtr<AOrderNoteActor>* Note = OrderNoteActors.Find(Number))
        {
            if (Note->IsValid())
            {
                (*Note)->Destroy();
            }
        }
        OrderNoteActors.Remove(Number);
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Заказ %d исполнен, исход наутро"), Number);
    return true;
}

bool AGridWorldManager::RefuseOrder(int32 Number)
{
    FActiveOrder* Order = FindActiveOrder(Number);
    if (!Order || Order->State != EOrderState::Open)
    {
        return false;
    }
    // Отказ ничего не стоит и ничего не меняет (§24.6-24.7).
    if (const TWeakObjectPtr<AOrderNoteActor>* Note = OrderNoteActors.Find(Number))
    {
        if (Note->IsValid())
        {
            (*Note)->Destroy();
        }
    }
    OrderNoteActors.Remove(Number);
    ActiveOrders.RemoveAll([Number](const FActiveOrder& Candidate) { return Candidate.Number == Number; });
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Заказ %d отвергнут"), Number);
    return true;
}

void AGridWorldManager::ChangeMolvaByOrder(float Delta)
{
    Molva = FMath::Clamp(Molva + Delta, -1.0f, 1.0f);
}

void AGridWorldManager::UpdateOrders()
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const double DayLength = GetOrderDayLengthSeconds();

    // Записки -- раз в игровые сутки (§24.2), не больше MaxOpenOrders разом.
    // Перемотка часов назад (SetGameClock) новых записок не даёт: сутки
    // запоминаются, выдача -- только когда наступили следующие.
    const int32 Day = FMath::FloorToInt(GameClockSeconds / DayLength);
    if (Day < LastOrderDay)
    {
        LastOrderDay = Day;
    }
    else if (Day > LastOrderDay)
    {
        LastOrderDay = Day;
        const int32 MinPerDay = Settings ? Settings->OrdersPerDayMin : 1;
        const int32 MaxPerDay = Settings ? Settings->OrdersPerDayMax : 2;
        const int32 MaxOpen = Settings ? Settings->MaxOpenOrders : 3;
        const int32 Wanted = WorldRNG.RandRange(FMath::Min(MinPerDay, MaxPerDay), FMath::Max(MinPerDay, MaxPerDay));
        for (int32 Index = 0; Index < Wanted; ++Index)
        {
            const int32 Open = ActiveOrders.FilterByPredicate([](const FActiveOrder& Order) { return Order.State == EOrderState::Open; }).Num();
            if (Open >= MaxOpen)
            {
                break;
            }
            const FName Picked = PickOrderDefinition(WorldRNG);
            if (!Picked.IsNone())
            {
                IssueOrder(Picked);
            }
        }
    }

    // Непрочитанные записки после загрузки сейва -- снова у порога.
    for (const FActiveOrder& Order : ActiveOrders)
    {
        // Карточки нет (сейв со старым каталогом) -- записку не ставить: её
        // нельзя прочесть, и она вставала бы снова каждый кадр.
        if (Order.State == EOrderState::Open && !Order.bNoteRead && HerbalistOrders::FindOrderDefinition(Order.DefinitionID))
        {
            SpawnOrderNote(Order);
        }
    }

    ResolveDueOrders();
}

void AGridWorldManager::ResolveDueOrders()
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float EdgeMargin = Settings ? Settings->OrderEdgeMargin : 0.05f;
    const float MinMagnitude = Settings ? Settings->OrderMinMagnitude : 0.05f;
    const double DayLength = GetOrderDayLengthSeconds();

    TArray<FActiveOrder> Done;
    for (const FActiveOrder& Order : ActiveOrders)
    {
        const bool bExpired = Order.State == EOrderState::Open && GameClockSeconds >= Order.DeadlineClock;
        const bool bResolved = Order.State == EOrderState::Delivered && GameClockSeconds >= Order.ResolveClock;
        if (bExpired || bResolved)
        {
            Done.Add(Order);
        }
    }

    for (const FActiveOrder& Order : Done)
    {
        ActiveOrders.RemoveAll([&Order](const FActiveOrder& Candidate) { return Candidate.Number == Order.Number; });
        const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(Order.DefinitionID);
        if (!Def)
        {
            continue;
        }
        const bool bDark = Def->Circle == EOrderCircle::Outlaws;

        if (Order.State == EOrderState::Open)
        {
            // Просрочен: добрый проситель помнит, лихой -- нет (§24.6).
            if (const TWeakObjectPtr<AOrderNoteActor>* Note = OrderNoteActors.Find(Order.Number))
            {
                if (Note->IsValid())
                {
                    (*Note)->Destroy();
                }
            }
            OrderNoteActors.Remove(Order.Number);
            if (!bDark)
            {
                ChangeMolvaByOrder(-(Settings ? Settings->OrderMolvaFailPenalty : 0.02f));
                PostCommunityNote(FText::Format(NSLOCTEXT("Orders", "Expired", "Заказ {0} так и не исполнен. Не дождались."), FText::AsNumber(Order.Number)));
            }
            continue;
        }

        float Distance = 0.0f;
        const EOrderMatch Match = HerbalistOrders::EvaluateOrderMatch(*Def, Order.DeliveredState, EdgeMargin, MinMagnitude, Distance);
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Заказ %d (%s): %s, отклонение %.2f"), Order.Number, *Def->ID.ToString(),
            Match == EOrderMatch::Exact ? TEXT("точно") : Match == EOrderMatch::Edge ? TEXT("сойдёт") : TEXT("мимо"), Distance);

        if (!bDark)
        {
            if (Match == EOrderMatch::Miss)
            {
                ChangeMolvaByOrder(-(Settings ? Settings->OrderMolvaFailPenalty : 0.02f));
                PostCommunityNote(FText::Format(NSLOCTEXT("Orders", "Failed", "Заказ {0}: не помогло."), FText::AsNumber(Order.Number)));
                continue;
            }
            ChangeMolvaByOrder(Match == EOrderMatch::Exact
                ? (Settings ? Settings->OrderMolvaExactGain : 0.03f)
                : (Settings ? Settings->OrderMolvaEdgeGain : 0.015f));
            GiveOrderPayment(Def->Payment);
            if (Match == EOrderMatch::Exact)
            {
                GiveOrderPayment(Def->ExactBonus);
            }
            continue;
        }

        if (Match != EOrderMatch::Miss)
        {
            // Тёмный заказ исполнен: слава падает, плата -- краденым, в мире
            // через одни-трое суток что-то случится (§24.6, §24.8).
            ChangeMolvaByOrder(-(Settings ? Settings->OrderMolvaDarkPenalty : 0.04f));
            GiveOrderPayment(Def->Payment);
            if (Match == EOrderMatch::Exact)
            {
                GiveOrderPayment(Def->ExactBonus);
            }
            if (Def->Consequence != EOrderConsequence::None)
            {
                const int32 MinDays = Settings ? Settings->OrderConsequenceDelayDaysMin : 1;
                const int32 MaxDays = Settings ? Settings->OrderConsequenceDelayDaysMax : 3;
                FPendingOrderConsequence& Pending = PendingOrderConsequences.AddDefaulted_GetRef();
                Pending.DefinitionID = Def->ID;
                Pending.Kind = Def->Consequence;
                Pending.FireClock = GameClockSeconds + WorldRNG.RandRange(FMath::Min(MinDays, MaxDays), FMath::Max(MinDays, MaxDays)) * DayLength;
            }
            continue;
        }

        // Обман лихого (§24.7): вскрывается с вероятностью, равной отклонению
        // от заказа. Молва не меняется -- люди об этом не знают.
        if (WorldRNG.FRand() < Distance)
        {
            // Месть -- как последствие: ложится, когда дом загружен.
            FPendingOrderConsequence& Revenge = PendingOrderConsequences.AddDefaulted_GetRef();
            Revenge.DefinitionID = Def->ID;
            Revenge.Kind = EOrderConsequence::Revenge;
            Revenge.FireClock = GameClockSeconds;
        }
        else
        {
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Обман по заказу %d не вскрылся"), Order.Number);
        }
    }

    // Отложенные последствия тёмных заказов (§24.8) и месть (§24.7) -- только
    // когда клетки у дома загружены: иначе запись в них пропала бы молча, а
    // слух в Травнике остался бы. Дом не расставлен -- ложатся слухом.
    const bool bCanFire = !HerbalistCore::IsValidCell(ZaryanaCell) || IsHomeAreaLoaded();
    TArray<FPendingOrderConsequence> Fired;
    for (const FPendingOrderConsequence& Pending : PendingOrderConsequences)
    {
        if (bCanFire && GameClockSeconds >= Pending.FireClock)
        {
            Fired.Add(Pending);
        }
    }
    PendingOrderConsequences.RemoveAll([this, bCanFire](const FPendingOrderConsequence& Pending)
    {
        return bCanFire && GameClockSeconds >= Pending.FireClock;
    });
    for (const FPendingOrderConsequence& Pending : Fired)
    {
        ApplyOrderConsequence(Pending);
    }
}

bool AGridWorldManager::IsHomeAreaLoaded() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = FMath::Max(Settings ? Settings->OrderPoisonRadiusCells : 2, Settings ? Settings->OrderCurseRadiusCells : 3);
    for (int32 DY = -Radius; DY <= Radius; DY += Radius)
    {
        for (int32 DX = -Radius; DX <= Radius; DX += Radius)
        {
            // Клетка за краем мира -- не помеха; незагруженная страница -- ждать.
            if (IsCellInGrid(ZaryanaCell.X + DX, ZaryanaCell.Y + DY) && !GetCellConst(ZaryanaCell.X + DX, ZaryanaCell.Y + DY))
            {
                return false;
            }
        }
    }
    return GetCellConst(ZaryanaCell.X, ZaryanaCell.Y) != nullptr;
}

void AGridWorldManager::AddStateAroundHome(int32 RadiusCells, float CorruptionDelta, float DistortionDelta)
{
    if (!HerbalistCore::IsValidCell(ZaryanaCell))
    {
        return;
    }
    for (int32 DY = -RadiusCells; DY <= RadiusCells; ++DY)
    {
        for (int32 DX = -RadiusCells; DX <= RadiusCells; ++DX)
        {
            if (FGridCell* Cell = GetCell(ZaryanaCell.X + DX, ZaryanaCell.Y + DY))
            {
                Cell->State.Meta.Corruption = FMath::Clamp(Cell->State.Meta.Corruption + CorruptionDelta, 0.0f, 1.0f);
                Cell->State.Meta.Distortion = FMath::Clamp(Cell->State.Meta.Distortion + DistortionDelta, 0.0f, 1.0f);
                MarkCellDirty(Cell->X, Cell->Y);
            }
        }
    }
}

void AGridWorldManager::ApplyOrderConsequence(const FPendingOrderConsequence& Pending)
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    switch (Pending.Kind)
    {
    case EOrderConsequence::Poison:
        AddStateAroundHome(Settings ? Settings->OrderPoisonRadiusCells : 2, Settings ? Settings->OrderPoisonCorruption : 0.3f, 0.0f);
        break;
    case EOrderConsequence::Charm:
        AddStateAroundHome(Settings ? Settings->OrderPoisonRadiusCells : 2, 0.0f, Settings ? Settings->OrderCharmDistortion : 0.1f);
        break;
    case EOrderConsequence::CurseNeighbour:
        if (HerbalistCore::IsValidCell(ZaryanaCell))
        {
            // Одна клетка у дома -- «куда указал заказчик», не сам дом.
            const int32 Radius = Settings ? Settings->OrderCurseRadiusCells : 3;
            int32 DX = WorldRNG.RandRange(-Radius, Radius);
            const int32 DY = WorldRNG.RandRange(-Radius, Radius);
            if (DX == 0 && DY == 0)
            {
                DX = 1;
            }
            if (FGridCell* Cell = GetCell(ZaryanaCell.X + DX, ZaryanaCell.Y + DY))
            {
                Cell->State.Meta.Corruption = FMath::Clamp(Cell->State.Meta.Corruption + (Settings ? Settings->OrderCurseCorruption : 0.4f), 0.0f, 1.0f);
                MarkCellDirty(Cell->X, Cell->Y);
            }
        }
        break;
    case EOrderConsequence::Revenge:
        ApplyOrderRevenge();
        return;   // свой текст -- в ApplyOrderRevenge
    default:
        break;   // SleepTheft -- только слух
    }
    if (const FOrderDefinition* Def = HerbalistOrders::FindOrderDefinition(Pending.DefinitionID))
    {
        PostCommunityNote(Def->ConsequenceRumor);
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Последствие заказа %s сработало"), *Pending.DefinitionID.ToString());
}

void AGridWorldManager::ApplyOrderRevenge()
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    // Кража из домашнего хранилища -- если оно есть и в нём что-то лежит;
    // иначе (или по броску) -- порча на пороге.
    if (WorldRNG.FRand() < (Settings ? Settings->OrderRevengeTheftChance : 0.5f) && GetWorld())
    {
        for (TActorIterator<AStorageContainer> It(GetWorld()); It; ++It)
        {
            if (!It->bIsHomeStorage || !It->InventoryComponent)
            {
                continue;
            }
            const TArray<FInventoryItem> Items = It->InventoryComponent->GetItems();
            if (Items.Num() == 0)
            {
                continue;
            }
            const int32 Stolen = WorldRNG.RandRange(0, Items.Num() - 1);
            It->InventoryComponent->RemoveItem(Stolen, Items[Stolen].Count);
            PostCommunityNote(NSLOCTEXT("Orders", "RevengeTheft", "Ночью кто-то побывал в погребе. Обман не прощают."));
            UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Обман вскрылся: кража из хранилища"));
            return;
        }
    }
    AddStateAroundHome(1, Settings ? Settings->OrderRevengeDoorstepCorruption : 0.2f, 0.0f);
    PostCommunityNote(NSLOCTEXT("Orders", "RevengeDoorstep", "У порога рассыпано что-то чёрное, трава вокруг полегла. Обман не прощают."));
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Обман вскрылся: порча на пороге"));
}

void AGridWorldManager::GiveOrderPayment(const FOrderPayment& Payment)
{
    if (Payment.ItemID.IsNone() || Payment.Count <= 0 || !GetWorld())
    {
        return;
    }
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* Registry = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = Registry ? Registry->GetRow(Payment.ItemID) : nullptr;
    if (!PC || !PC->InventoryComponent || !Row)
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] Плата %s ×%d некуда положить"), *Payment.ItemID.ToString(), Payment.Count);
        return;
    }
    FInventoryItem Item;
    Item.IngredientID = Payment.ItemID;
    Item.State = Row->BaseState;
    Item.Count = Payment.Count;
    Item.CreationTime = static_cast<float>(GameClockSeconds);
    if (PC->InventoryComponent->AddItem(Item, Payment.Count))
    {
        return;
    }
    // Котомка полна -- плата ложится в домашнее хранилище, а не пропадает.
    for (TActorIterator<AStorageContainer> It(GetWorld()); It; ++It)
    {
        if (It->bIsHomeStorage && It->InventoryComponent && It->InventoryComponent->AddItem(Item, Payment.Count))
        {
            PostCommunityNote(NSLOCTEXT("Orders", "PaidToCellar", "Котомка полна -- плату оставили в погребе."));
            return;
        }
    }
    PostCommunityNote(NSLOCTEXT("Orders", "PaymentLost", "Плату оставили у порога, но положить её было некуда -- к утру её не стало."));
}

void AGridWorldManager::PostCommunityNote(const FText& Text)
{
    if (Text.IsEmpty() || !GetWorld())
    {
        return;
    }
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Orders] %s"), *Text.ToString());
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld()->GetFirstPlayerController());
    if (!PC || !PC->JournalComponent)
    {
        return;
    }
    FJournalEntry Entry;
    Entry.Type = EJournalEntryType::CommunityNote;
    Entry.FragmentText = Text;
    Entry.Cell = ZaryanaCell;
    Entry.bWasNight = IsNight();
    Entry.GameTimeSeconds = static_cast<float>(GameClockSeconds);
    PC->JournalComponent->AddEntry(Entry);
}

void AGridWorldManager::SetOrdersState(const TArray<FActiveOrder>& InOrders, const TArray<FPendingOrderConsequence>& InPending,
    int32 InNextNumber, int32 InLastDay)
{
    for (const TPair<int32, TWeakObjectPtr<AOrderNoteActor>>& Pair : OrderNoteActors)
    {
        if (Pair.Value.IsValid())
        {
            Pair.Value->Destroy();
        }
    }
    OrderNoteActors.Reset();
    ActiveOrders = InOrders;
    PendingOrderConsequences = InPending;
    NextOrderNumber = FMath::Max(1, InNextNumber);
    LastOrderDay = InLastDay;
}

// ============================================================================
// Тайники (AOrderCacheActor, решение пользователя 2026-09-21)
// ============================================================================

void AGridWorldManager::RegisterOrderCache(AOrderCacheActor* Cache)
{
    if (Cache)
    {
        OrderCaches.AddUnique(Cache);
    }
}

void AGridWorldManager::UnregisterOrderCache(AOrderCacheActor* Cache)
{
    OrderCaches.RemoveAll([Cache](const TWeakObjectPtr<AOrderCacheActor>& Entry)
    {
        return !Entry.IsValid() || Entry.Get() == Cache;
    });
}

bool AGridWorldManager::HasAnyOrderCache() const
{
    return OrderCaches.ContainsByPredicate([](const TWeakObjectPtr<AOrderCacheActor>& Entry) { return Entry.IsValid(); });
}

bool AGridWorldManager::IsDeliveryAllowedAt(const FVector& Location) const
{
    // Тайников ещё не расставили -- правило не действует.
    if (!HasAnyOrderCache())
    {
        return true;
    }
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float ReachCm = (Settings ? Settings->OrderCacheReachMeters : 3.0f) * 100.0f;
    // Какой тайник -- неважно: подходит любой (решение пользователя).
    for (const TWeakObjectPtr<AOrderCacheActor>& Entry : OrderCaches)
    {
        const AOrderCacheActor* Cache = Entry.Get();
        if (Cache && FVector::DistSquared(Cache->GetActorLocation(), Location) <= FMath::Square(ReachCm))
        {
            return true;
        }
    }
    return false;
}


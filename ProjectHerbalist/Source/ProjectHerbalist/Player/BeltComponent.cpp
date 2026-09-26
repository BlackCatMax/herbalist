// BeltComponent.cpp
#include "Player/BeltComponent.h"
#include "Player/BeltItemActor.h"
#include "Player/HeldItemComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HerbalistLogChannels.h"

namespace
{
    // Пояс -- ниже и чуть впереди взгляда; места слева направо: инструмент,
    // контейнер, оберег, мешочек.
    const FVector BeltOffset(22.0f, 0.0f, -62.0f);
    constexpr float BeltSlotOffsetsY[] = { -22.0f, -8.0f, 8.0f, 22.0f };
    constexpr float BeltRefreshSeconds = 0.25f;
    constexpr float LitBrightness = 2.0f;
    constexpr float DimBrightness = 0.35f;

    const FName SilverWardID(TEXT("Серебряный оберег"));

    const FIngredientTableRow* FindRow(const APlayerController* PC, FName ID)
    {
        const UGameInstance* GI = PC ? PC->GetGameInstance() : nullptr;
        const UIngredientRegistrySubsystem* Registry = GI ? GI->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
        return Registry ? Registry->GetRow(ID) : nullptr;
    }
}

UBeltComponent::UBeltComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

AHerbalistPlayerController* UBeltComponent::GetController() const
{
    return Cast<AHerbalistPlayerController>(GetOwner());
}

void UBeltComponent::BeginPlay()
{
    Super::BeginPlay();
}

void UBeltComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    ClearVisual();
    Super::EndPlay(Reason);
}

void UBeltComponent::SetContainerItem(FName ItemID)
{
    ContainerItemID = ItemID;
}

bool UBeltComponent::IsInView() const
{
    const APlayerController* PC = GetController();
    if (!PC) return false;
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    return FRotator::NormalizeAxis(ViewRotation.Pitch) <= BeltPitchDegrees;
}

bool UBeltComponent::OwnsItem(FName ItemID) const
{
    const AHerbalistPlayerController* PC = GetController();
    if (!PC || !PC->InventoryComponent || ItemID.IsNone()) return false;
    for (const FInventoryItem& Item : PC->InventoryComponent->ViewItems())
    {
        if (Item.IngredientID == ItemID && Item.Count > 0) return true;
    }
    return false;
}

FName UBeltComponent::GetSlotItem(EBeltSlot Slot) const
{
    const AHerbalistPlayerController* PC = GetController();
    if (!PC) return NAME_None;
    switch (Slot)
    {
    case EBeltSlot::Tool:
        return AHerbalistPlayerController::ItemForGatheringTool(PC->CurrentGatheringTool);
    case EBeltSlot::Container:
    {
        if (!PC->InventoryComponent || PC->InventoryComponent->ContainerType == EStorageContainerType::None) return NAME_None;
        // После загрузки тип контейнера восстановлен, а какой предмет его
        // дал -- нет: ищем в котомке по карточке. Без реестра -- что надели.
        for (const FInventoryItem& Item : PC->InventoryComponent->ViewItems())
        {
            const FIngredientTableRow* Row = FindRow(PC, Item.IngredientID);
            if (Row && Row->GrantsContainerType == PC->InventoryComponent->ContainerType) return Item.IngredientID;
        }
        return ContainerItemID;
    }
    case EBeltSlot::Ward:
    {
        if (!WardItemID.IsNone()) return WardItemID;
        // Серебряный оберег действует и после загрузки (флаг мира
        // сохраняется), а что он висел на поясе -- нет: выводим из мира, как
        // инструмент и контейнер (ревью 2026-09-21).
        const AGridWorldManager* Grid = PC->FindWorldManager();
        return (Grid && Grid->IsSilverWardActive() && OwnsItem(SilverWardID)) ? SilverWardID : NAME_None;
    }
    case EBeltSlot::SeedPouch:
        return FName(TEXT("Семенной мешочек"));
    default:
        return NAME_None;
    }
}

bool UBeltComponent::IsWardLit() const
{
    const FName Ward = GetSlotItem(EBeltSlot::Ward);
    if (Ward.IsNone()) return false;
    const AHerbalistPlayerController* PC = GetController();
    const AGridWorldManager* Grid = PC ? PC->FindWorldManager() : nullptr;
    if (Ward == SilverWardID) return Grid && Grid->IsSilverWardActive();
    return Grid && Grid->GetGameClockSeconds() < WardLitUntil;
}

bool UBeltComponent::PutOn(int32 InventoryIndex)
{
    AHerbalistPlayerController* PC = GetController();
    if (!PC || !PC->InventoryComponent || !PC->InventoryComponent->GetItems().IsValidIndex(InventoryIndex)) return false;
    const FName ID = PC->InventoryComponent->GetItems()[InventoryIndex].IngredientID;
    AGridWorldManager* Grid = PC->FindWorldManager();

    // Инструмент -- по таблице контроллера, без реестра.
    EGatheringTool Tool;
    if (AHerbalistPlayerController::GatheringToolForItem(ID, Tool))
    {
        PC->CurrentGatheringTool = Tool;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: инструмент '%s'"), *ID.ToString());
        if (PC->HeldItemComponent) PC->HeldItemComponent->PutAway();
        return true;
    }

    // Серебряный оберег -- действует, пока висит.
    if (ID == SilverWardID)
    {
        PC->EquipSilverWard();
        if (Grid && Grid->IsSilverWardActive())
        {
            WardItemID = ID;
            if (PC->HeldItemComponent) PC->HeldItemComponent->PutAway();
        }
        return true;
    }

    const FIngredientTableRow* Row = FindRow(PC, ID);
    if (!Row) return false;

    if (Row->bIsWard && Row->WardEffectType != EWardEffectType::None)
    {
        // Повесить -- включить на срок, тем же путём, что команда.
        if (PC->ActivateWardFromItem(ID))
        {
            if (GetSlotItem(EBeltSlot::Ward) == SilverWardID && Grid)
            {
                Grid->SetSilverWardActive(false);
            }
            WardItemID = ID;
            const UHerbalistSettings* Settings = GetHerbalistSettings();
            // Тиражный оберег срока не имеет -- светится, пока висит.
            WardLitUntil = Row->bIsTieredWard ? TNumericLimits<double>::Max()
                : (Grid ? Grid->GetGameClockSeconds() : 0.0) + (Settings ? Settings->WardDurationSeconds : 600.0f);
            UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: оберег '%s' повешен"), *ID.ToString());
            if (PC->HeldItemComponent) PC->HeldItemComponent->PutAway();
        }
        return true;
    }

    if (Row->GrantsContainerType != EStorageContainerType::None)
    {
        if (PC->EquipContainerFromItem(ID))
        {
            ContainerItemID = ID;
            if (PC->HeldItemComponent) PC->HeldItemComponent->PutAway();
        }
        return true;
    }
    return false;
}

void UBeltComponent::TakeIntoHand(FName ItemID)
{
    AHerbalistPlayerController* PC = GetController();
    if (!PC || !PC->InventoryComponent || !PC->HeldItemComponent) return;
    const TArray<FInventoryItem> Items = PC->InventoryComponent->GetItems();
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        if (Items[Index].IngredientID == ItemID && Items[Index].Count > 0)
        {
            PC->HeldItemComponent->TakeFromInventory(Index);
            return;
        }
    }
}

void UBeltComponent::UseSlot(EBeltSlot Slot)
{
    AHerbalistPlayerController* PC = GetController();
    if (!PC) return;
    const FName ID = GetSlotItem(Slot);
    switch (Slot)
    {
    case EBeltSlot::Tool:
        if (ID.IsNone()) return;
        PC->CurrentGatheringTool = EGatheringTool::BareHands;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: '%s' снят -- сбор голыми руками"), *ID.ToString());
        TakeIntoHand(ID);
        break;
    case EBeltSlot::Container:
        if (ID.IsNone() || !PC->InventoryComponent) return;
        PC->InventoryComponent->ContainerType = EStorageContainerType::None;
        ContainerItemID = NAME_None;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: '%s' снят"), *ID.ToString());
        TakeIntoHand(ID);
        break;
    case EBeltSlot::Ward:
        if (ID.IsNone()) return;
        if (ID == SilverWardID)
        {
            if (AGridWorldManager* Grid = PC->FindWorldManager())
            {
                Grid->SetSilverWardActive(false);
            }
        }
        // Оберег на срок, снятый, дорабатывает своё окно: снять -- не значит
        // погасить уже данное.
        WardItemID = NAME_None;
        WardLitUntil = -1.0;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: оберег '%s' снят"), *ID.ToString());
        TakeIntoHand(ID);
        break;
    case EBeltSlot::SeedPouch:
        PC->CurrentHarvestIntent = PC->CurrentHarvestIntent == EHarvestIntent::Seed ? EHarvestIntent::Brew : EHarvestIntent::Seed;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: семенной мешочек %s"),
            PC->CurrentHarvestIntent == EHarvestIntent::Seed ? TEXT("развязан -- сбор на семена") : TEXT("завязан -- обычный сбор"));
        break;
    default:
        break;
    }
    SyncVisual();
}

void UBeltComponent::ReleaseMissing()
{
    AHerbalistPlayerController* PC = GetController();
    if (!PC || !PC->InventoryComponent) return;

    const FName ToolID = GetSlotItem(EBeltSlot::Tool);
    if (!ToolID.IsNone() && !OwnsItem(ToolID))
    {
        PC->CurrentGatheringTool = EGatheringTool::BareHands;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: '%s' ушёл из котомки -- сбор голыми руками"), *ToolID.ToString());
    }

    const FName ContainerID = GetSlotItem(EBeltSlot::Container);
    if (!ContainerID.IsNone() && !OwnsItem(ContainerID))
    {
        PC->InventoryComponent->ContainerType = EStorageContainerType::None;
        ContainerItemID = NAME_None;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Belt: '%s' ушёл из котомки -- контейнера нет"), *ContainerID.ToString());
    }

    const FName WardID = GetSlotItem(EBeltSlot::Ward);
    if (!WardID.IsNone() && !OwnsItem(WardID))
    {
        if (WardID == SilverWardID)
        {
            if (AGridWorldManager* Grid = PC->FindWorldManager())
            {
                Grid->SetSilverWardActive(false);
            }
        }
        WardItemID = NAME_None;
        WardLitUntil = -1.0;
    }
}

ABeltItemActor* UBeltComponent::FindItemUnderView(const FVector& Start, const FVector& End) const
{
    if (!IsInView()) return nullptr;
    ABeltItemActor* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HerbalistBeltPick), false);
    for (ABeltItemActor* Actor : Shown)
    {
        UPrimitiveComponent* Shape = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
        FHitResult Hit;
        if (Shape && Shape->LineTraceComponent(Hit, Start, End, Params))
        {
            const float DistSq = FVector::DistSquared(Start, Hit.ImpactPoint);
            if (DistSq < BestDistSq)
            {
                BestDistSq = DistSq;
                Best = Actor;
            }
        }
    }
    return Best;
}

void UBeltComponent::ClearVisual()
{
    for (ABeltItemActor* Actor : Shown)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    Shown.Reset();
}

void UBeltComponent::SyncVisual()
{
    AHerbalistPlayerController* PC = GetController();
    UWorld* World = GetWorld();
    if (!PC || !PC->InventoryComponent || !World || !IsInView())
    {
        ClearVisual();
        return;
    }

    // Заглушки живут, пока пояс виден, и обновляются на месте (ревью
    // 2026-09-21): пересоздание четыре раза в секунду роняло подсветку того,
    // на что смотрят. Пересоздаётся только слот, что опустел или появился.
    TArray<TObjectPtr<ABeltItemActor>> Kept;
    for (int32 SlotIndex = 0; SlotIndex < static_cast<int32>(EBeltSlot::Count); ++SlotIndex)
    {
        const EBeltSlot Slot = static_cast<EBeltSlot>(SlotIndex);
        const FName ID = GetSlotItem(Slot);
        TObjectPtr<ABeltItemActor>* Existing = Shown.FindByPredicate([Slot](const TObjectPtr<ABeltItemActor>& Actor) { return Actor && Actor->GetSlot() == Slot; });
        ABeltItemActor* Actor = Existing ? Existing->Get() : nullptr;
        if (ID.IsNone())
        {
            if (Actor) Actor->Destroy();
            continue;
        }

        // Вид -- тот же предмет из котомки (его состояние даёт цвет);
        // мешочек -- сам по себе: развязан -- зелёный (Природа), завязан --
        // другого цвета (при равных осях ColorForState берёт Тело).
        FInventoryItem Look;
        Look.IngredientID = ID;
        if (Slot == EBeltSlot::SeedPouch)
        {
            const bool bOpen = PC->CurrentHarvestIntent == EHarvestIntent::Seed;
            Look.State.Direction.Nature = bOpen ? 1.0f : 0.25f;
            Look.State.Direction.Body = Look.State.Direction.Mind = Look.State.Direction.Spirit = bOpen ? 0.0f : 0.25f;
        }
        else
        {
            const int32 Index = PC->InventoryComponent->GetItems().IndexOfByPredicate([ID](const FInventoryItem& Item) { return Item.IngredientID == ID; });
            if (Index != INDEX_NONE) Look = PC->InventoryComponent->GetItems()[Index];
        }

        if (!Actor)
        {
            FActorSpawnParameters Params;
            Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
            Actor = World->SpawnActor<ABeltItemActor>(ABeltItemActor::StaticClass(), FTransform::Identity, Params);
            if (!Actor) continue;
            Actor->SetSlot(Slot);
            Actor->SetActorScale3D(FVector(0.06f));
        }
        const bool bHard = Slot == EBeltSlot::Tool || Slot == EBeltSlot::Ward;
        Actor->ShowItem(Look, bHard, false);
        if (Slot == EBeltSlot::Ward)
        {
            Actor->SetBrightness(IsWardLit() ? LitBrightness : DimBrightness);
        }
        Kept.Add(Actor);
    }
    Shown = MoveTemp(Kept);
    VisualRefreshAccumulator = 0.0f;
}

void UBeltComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    // Что ушло из котомки -- тем же тактом, что обновление заглушек, не каждый
    // кадр (аудит 2026-09-26, П1): отпустить предмет на четверть секунды
    // позже никто не заметит.
    ReleaseAccumulator += DeltaTime;
    if (ReleaseAccumulator >= BeltRefreshSeconds)
    {
        ReleaseAccumulator = 0.0f;
        ReleaseMissing();
    }

    if (!IsInView())
    {
        ClearVisual();
        return;
    }

    // Пересборка четырёх заглушек -- раз в четверть секунды: оберег гаснет
    // по часам, котомка меняется без сигнала пояса.
    VisualRefreshAccumulator += DeltaTime;
    if (Shown.Num() == 0 || VisualRefreshAccumulator >= BeltRefreshSeconds)
    {
        SyncVisual();
    }

    APlayerController* PC = GetController();
    if (!PC) return;
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FRotator Yaw(0.0f, ViewRotation.Yaw, 0.0f);
    const FVector Base = ViewLocation + Yaw.RotateVector(BeltOffset);
    for (ABeltItemActor* Actor : Shown)
    {
        if (!Actor) continue;
        const int32 SlotIndex = static_cast<int32>(Actor->GetSlot());
        Actor->SetActorLocation(Base + Yaw.RotateVector(FVector(0.0f, BeltSlotOffsetsY[SlotIndex], 0.0f)));
        Actor->SetActorRotation(Yaw);
    }
}

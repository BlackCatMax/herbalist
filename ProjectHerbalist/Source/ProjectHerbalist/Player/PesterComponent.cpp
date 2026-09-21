// PesterComponent.cpp
#include "Player/PesterComponent.h"
#include "Player/PesterItemActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Types/HerbalistNameUtils.h"
#include "UI/InventorySlotWidget.h"
#include "UI/SensationLineWidget.h"
#include "Components/PrimitiveComponent.h"
#include "Engine/World.h"

namespace
{
    // Пестерь перед камерой: чуть ниже взгляда, чтобы в него надо было
    // посмотреть вниз, как в настоящий короб у груди.
    const FVector PesterOffset(55.0f, 0.0f, -28.0f);
    // Мешочки -- в ряд слева направо, внутри мешочка предметы -- рядами
    // вглубь и вверх.
    constexpr float PouchSpacing = 13.0f;
    constexpr float ItemSpacing = 6.0f;
    constexpr int32 ItemsPerRow = 2;

    const FName PotionID(TEXT("Potion"));
}

UPesterComponent::UPesterComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

EPesterPouch UPesterComponent::PouchForItem(const FInventoryItem& Item, EIngredientClass Class, bool bClassKnown)
{
    // Зелье и вода -- склянки, чем бы ни было записано в реестре.
    if (Item.bIsWater || Item.IngredientID == PotionID) return EPesterPouch::Vials;
    if (!bClassKnown) return EPesterPouch::Other;
    switch (Class)
    {
    case EIngredientClass::Plant:   return EPesterPouch::Herbs;
    case EIngredientClass::Fungus:  return EPesterPouch::Fungi;
    case EIngredientClass::Mineral: return EPesterPouch::Stones;
    case EIngredientClass::Water:
    case EIngredientClass::Essence: return EPesterPouch::Vials;
    default:                        return EPesterPouch::Other;
    }
}

void UPesterComponent::BeginPlay()
{
    Super::BeginPlay();
    if (const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner()))
    {
        if (PC->InventoryComponent)
        {
            PC->InventoryComponent->OnInventoryChanged.AddDynamic(this, &UPesterComponent::OnInventoryChanged);
        }
    }
}

void UPesterComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    // Сначала закрыть: закрытие раскладки хранилища возвращает подписку на
    // котомку, её и снимаем следом.
    Close();
    if (const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner()))
    {
        if (PC->InventoryComponent)
        {
            PC->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UPesterComponent::OnInventoryChanged);
        }
    }
    ClearLayout();
    bOpen = false;
    Super::EndPlay(Reason);
}

void UPesterComponent::Open()
{
    if (bOpen) return;
    bOpen = true;
    Rebuild();
}

void UPesterComponent::OpenContainer(AStorageContainer* Container)
{
    if (!Container || !Container->InventoryComponent) return;
    Close();
    ViewedContainer = Container;
    Container->InventoryComponent->OnInventoryChanged.AddDynamic(this, &UPesterComponent::OnInventoryChanged);
    // Пока раскрыто хранилище, своя котомка раскладку не трогает (ревью
    // 2026-09-21): иначе каждый перенос перестраивал её дважды, а распад в
    // котомке -- ещё и без всякой причины.
    if (const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner()))
    {
        if (PC->InventoryComponent)
        {
            PC->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UPesterComponent::OnInventoryChanged);
        }
    }
    bOpen = true;
    Rebuild();
}

void UPesterComponent::UnbindContainer()
{
    if (ViewedContainer.IsExplicitlyNull()) return;
    if (AStorageContainer* Container = ViewedContainer.Get())
    {
        if (Container->InventoryComponent)
        {
            Container->InventoryComponent->OnInventoryChanged.RemoveDynamic(this, &UPesterComponent::OnInventoryChanged);
        }
    }
    ViewedContainer.Reset();
    if (const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner()))
    {
        if (PC->InventoryComponent && !PC->InventoryComponent->OnInventoryChanged.IsAlreadyBound(this, &UPesterComponent::OnInventoryChanged))
        {
            PC->InventoryComponent->OnInventoryChanged.AddDynamic(this, &UPesterComponent::OnInventoryChanged);
        }
    }
}

void UPesterComponent::Close()
{
    UnbindContainer();
    StatusFocus.Reset();
    StatusLine.Reset();
    if (StatusWidget)
    {
        StatusWidget->RemoveFromParent();
        StatusWidget = nullptr;
    }
    if (!bOpen) return;
    bOpen = false;
    ClearLayout();
}

UHerbalistInventoryComponent* UPesterComponent::GetSourceInventory() const
{
    if (!ViewedContainer.IsExplicitlyNull())
    {
        // Раскрытое хранилище исчезло (выгружено, разрушено) до тика, который
        // закроет раскладку, -- это не своя котомка (ревью 2026-09-21).
        const AStorageContainer* Container = ViewedContainer.Get();
        return Container ? Container->InventoryComponent : nullptr;
    }
    const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    return PC ? PC->InventoryComponent : nullptr;
}

void UPesterComponent::Toggle()
{
    if (bOpen)
    {
        Close();
    }
    else
    {
        Open();
    }
}

void UPesterComponent::OnInventoryChanged()
{
    // Закрытый пестерь не раскладывается: котомка меняется постоянно, а
    // смотреть в неё игрок сейчас не смотрит.
    if (bOpen)
    {
        Rebuild();
    }
}

void UPesterComponent::ClearLayout()
{
    for (APesterItemActor* Actor : LaidOut)
    {
        if (Actor)
        {
            Actor->Destroy();
        }
    }
    LaidOut.Reset();
    LocalOffsets.Reset();
}

void UPesterComponent::Rebuild()
{
    ClearLayout();

    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    UWorld* World = GetWorld();
    UHerbalistInventoryComponent* Source = GetSourceInventory();
    if (!PC || !Source || !World) return;
    // Предметы хранилища показываются воспринятыми, как их показывала
    // подсказка окна переноса (UInventorySlotWidget::ResolvePerceivedItem).
    const bool bContainer = ViewedContainer.IsValid();
    const AGridWorldManager* Grid = bContainer ? PC->FindWorldManager() : nullptr;
    const float Clarity = Grid ? Grid->GetGlobalPerceptionClarity() : 0.0f;

    const UIngredientRegistrySubsystem* Registry = nullptr;
    if (const UGameInstance* GI = PC->GetGameInstance())
    {
        Registry = GI->GetSubsystem<UIngredientRegistrySubsystem>();
    }

    int32 CountInPouch[static_cast<int32>(EPesterPouch::Count)] = {};
    const TArray<FInventoryItem> Items = Source->GetItems();
    for (int32 Index = 0; Index < Items.Num(); ++Index)
    {
        const FInventoryItem& Item = Items[Index];
        if (Item.Count <= 0) continue;

        const FIngredientTableRow* Row = Registry ? Registry->GetRow(Item.IngredientID) : nullptr;
        const EIngredientClass Class = Row ? Row->Class : EIngredientClass::Unknown;
        const EPesterPouch Pouch = PouchForItem(Item, Class, Row != nullptr);
        const int32 PouchIndex = static_cast<int32>(Pouch);
        const int32 Slot = CountInPouch[PouchIndex]++;

        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        APesterItemActor* Actor = World->SpawnActor<APesterItemActor>(APesterItemActor::StaticClass(), FTransform::Identity, Params);
        if (!Actor) continue;

        const bool bMineral = Pouch == EPesterPouch::Stones;
        const bool bLiquid = Pouch == EPesterPouch::Vials;
        Actor->BindItem(Item, Index);
        Actor->ShowItem(bContainer ? UInventorySlotWidget::PerceiveSingleItem(Item, Clarity) : Item, bMineral, bLiquid);
        // Горсть и полный мешочек: крупнее стопка -- крупнее заглушка, но не
        // бесконечно.
        Actor->SetActorScale3D(FVector(0.04f + 0.008f * FMath::Min(Item.Count, 5)));

        // Мешочки в ряд по центру, внутри -- по два в ряд, ряды вглубь и вверх.
        const float PouchY = (PouchIndex - (static_cast<int32>(EPesterPouch::Count) - 1) * 0.5f) * PouchSpacing;
        const float InRowY = ((Slot % ItemsPerRow) - 0.5f) * ItemSpacing;
        const int32 RowNumber = Slot / ItemsPerRow;
        LocalOffsets.Add(FVector(RowNumber * ItemSpacing, PouchY + InRowY, RowNumber * 2.0f));
        LaidOut.Add(Actor);
    }
    PlaceItems();
}

APesterItemActor* UPesterComponent::FindItemUnderView(const FVector& Start, const FVector& End) const
{
    if (!bOpen) return nullptr;
    APesterItemActor* Best = nullptr;
    float BestDistSq = TNumericLimits<float>::Max();
    FCollisionQueryParams Params(SCENE_QUERY_STAT(HerbalistPesterPick), false);
    for (APesterItemActor* Actor : LaidOut)
    {
        UPrimitiveComponent* Shape = Actor ? Cast<UPrimitiveComponent>(Actor->GetRootComponent()) : nullptr;
        FHitResult Hit;
        // Луч прямо по форме заглушки, мимо каналов: так её видит только тот,
        // кто спросил, а не каждый луч мира.
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

void UPesterComponent::PlaceItems()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC) return;
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    // Пестерь держится по горизонтали взгляда: наклон головы вниз не должен
    // уводить короб вниз вместе с камерой -- в него смотрят сверху.
    const FRotator Yaw(0.0f, ViewRotation.Yaw, 0.0f);
    const FVector Base = ViewLocation + Yaw.RotateVector(PesterOffset);
    for (int32 i = 0; i < LaidOut.Num(); ++i)
    {
        if (APesterItemActor* Actor = LaidOut[i])
        {
            Actor->SetActorLocation(Base + Yaw.RotateVector(LocalOffsets[i]));
            Actor->SetActorRotation(Yaw);
        }
    }
}

void UPesterComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bOpen) return;

    // Открылось окно (стол, хранилище, Травник, заказы) -- пестерь
    // закрывается. Одно место вместо проверки в каждом окне (ревью
    // 2026-09-21): иначе после закрытия окна игрок внезапно видел бы
    // забытый пестерь перед носом.
    const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    if (PC && PC->bIsAnyWidgetOpen)
    {
        Close();
        return;
    }

    // Хранилище раскрыто, а игрок отошёл или хранилища больше нет --
    // раскладка закрывается: в руки из погреба на другом конце поляны не
    // берут.
    if (ViewedContainer.IsValid() || !ViewedContainer.IsExplicitlyNull())
    {
        const AStorageContainer* Container = ViewedContainer.Get();
        const APawn* Pawn = PC ? PC->GetPawn() : nullptr;
        // У голого AStorageContainer (без Blueprint-меша) нет корня, а значит
        // и места в мире -- мерить до него нечего, дальность не проверяется.
        const bool bPlaced = Container && Container->GetRootComponent();
        if (!Container || (Pawn && bPlaced && FVector::DistSquared(Pawn->GetActorLocation(), Container->GetActorLocation()) > FMath::Square(ContainerReachCm)))
        {
            Close();
            return;
        }
        UpdateStatusLine();
    }
    PlaceItems();
}

void UPesterComponent::UpdateStatusLine()
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    const AStorageContainer* Container = ViewedContainer.Get();
    if (!PC || !Container || !Container->InventoryComponent) return;

    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    APesterItemActor* Focus = FindItemUnderView(ViewLocation, ViewLocation + ViewRotation.Vector() * 200.0f);
    if (Focus == StatusFocus.Get()) return;
    StatusFocus = Focus;

    StatusLine.Reset();
    if (Focus)
    {
        const int32 Index = Container->InventoryComponent->FindItemIndex(Focus->GetItem(), Focus->GetIndexHint());
        if (const FInventoryItem* Real = Container->InventoryComponent->GetSlot(Index))
        {
            StatusLine = GetItemProcessStatus(*Real, Container->InventoryComponent->StationType);
        }
    }

    // Без вьюпорта (автотесты) строка только запоминается.
    if (StatusLine.IsEmpty() || !PC->IsLocalController() || !GetWorld() || !GetWorld()->GetGameViewport())
    {
        if (StatusWidget)
        {
            StatusWidget->RemoveFromParent();
            StatusWidget = nullptr;
        }
        return;
    }
    if (!StatusWidget)
    {
        StatusWidget = CreateWidget<USensationLineWidget>(PC, USensationLineWidget::StaticClass());
        if (StatusWidget)
        {
            StatusWidget->AddToViewport(5);
        }
    }
    if (StatusWidget)
    {
        StatusWidget->SetLine(StatusLine);
    }
}

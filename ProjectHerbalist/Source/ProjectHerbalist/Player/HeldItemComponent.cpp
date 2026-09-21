// HeldItemComponent.cpp
#include "Player/HeldItemComponent.h"
#include "Player/HeldItemActor.h"
#include "Player/HerbalistPlayerController.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Core/Journal/JournalTypes.h"
#include "Core/Types/HerbalistSensation.h"
#include "Core/Data/IngredientTableRow.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "UI/SensationLineWidget.h"
#include "Engine/World.h"

namespace
{
    // Где держать предмет относительно взгляда: опущенным в руке и поднесённым
    // к глазам (осмотр).
    const FVector HeldOffset(45.0f, 18.0f, -18.0f);
    const FVector InspectOffset(28.0f, 0.0f, -2.0f);
}

UHeldItemComponent::UHeldItemComponent()
{
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;
}

int32 UHeldItemComponent::ResolveHeldIndex() const
{
    const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    if (!bHolding || !PC || !PC->InventoryComponent) return INDEX_NONE;
    // Не точное сравнение: распад двигает состояние каждую секунду, и рука
    // роняла бы предмет прямо при осмотре (ревью 2026-09-21).
    return PC->InventoryComponent->FindItemIndex(HeldItem, HeldIndexHint);
}

bool UHeldItemComponent::TakeFromInventory(int32 InventoryIndex)
{
    const AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    if (!PC || !PC->InventoryComponent || !PC->InventoryComponent->GetItems().IsValidIndex(InventoryIndex))
    {
        return false;
    }
    HeldItem = PC->InventoryComponent->GetItems()[InventoryIndex];
    HeldIndexHint = InventoryIndex;
    bHolding = true;
    bInspecting = false;
    ShowSensation(false);
    SyncVisual();
    return true;
}

void UHeldItemComponent::PutAway()
{
    bHolding = false;
    bInspecting = false;
    ShowSensation(false);
    SyncVisual();
}

void UHeldItemComponent::ToggleInspect()
{
    if (!bHolding) return;
    bInspecting = !bInspecting;
    if (!bInspecting)
    {
        ShowSensation(false);
        return;
    }

    // То, что видит травник, а не настоящее: предмет в котомке уже несёт
    // воспринятое состояние (тот же State, что показывала подсказка).
    SensationLine = HerbalistSensation::Describe(HeldItem.State);
    ShowSensation(true);

    // Осмотр записывается в Травник, как сбор (DESIGN_Diegetic_Interface.md).
    AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetOwner());
    if (PC && PC->JournalComponent)
    {
        FJournalEntry Entry;
        Entry.Type = EJournalEntryType::Inspection;
        Entry.IngredientID = HeldItem.IngredientID;
        Entry.PerceivedState = HeldItem.State;
        Entry.FragmentText = FText::FromString(SensationLine);
        if (const AGridWorldManager* Grid = PC->FindWorldManager())
        {
            Entry.bWasNight = Grid->IsNight();
            Entry.GameTimeSeconds = static_cast<float>(Grid->GetGameClockSeconds());
        }
        PC->JournalComponent->AddEntry(Entry);
    }
}

void UHeldItemComponent::ShowSensation(bool bShow)
{
    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!bShow)
    {
        if (SensationWidget)
        {
            SensationWidget->RemoveFromParent();
            SensationWidget = nullptr;
        }
        return;
    }
    // Без мира с вьюпортом (автотесты) строка только запоминается.
    if (!PC || !PC->IsLocalController() || !GetWorld() || !GetWorld()->GetGameViewport()) return;
    if (!SensationWidget)
    {
        SensationWidget = CreateWidget<USensationLineWidget>(PC, USensationLineWidget::StaticClass());
        if (SensationWidget)
        {
            SensationWidget->AddToViewport(5);
        }
    }
    if (SensationWidget)
    {
        SensationWidget->SetLine(SensationLine);
    }
}

void UHeldItemComponent::SyncVisual()
{
    if (!bHolding)
    {
        if (HeldActor)
        {
            HeldActor->Destroy();
            HeldActor = nullptr;
        }
        return;
    }
    UWorld* World = GetWorld();
    if (!World) return;
    if (!HeldActor)
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
        HeldActor = World->SpawnActor<AHeldItemActor>(AHeldItemActor::StaticClass(), FTransform::Identity, Params);
    }
    if (!HeldActor) return;

    // Класс -- из реестра ингредиентов; без реестра (автотесты) -- по
    // признакам самого предмета.
    bool bMineral = false;
    bool bLiquid = HeldItem.bIsWater || HeldItem.IngredientID == FName(TEXT("Potion"));
    if (const APlayerController* PC = Cast<APlayerController>(GetOwner()))
    {
        if (const UGameInstance* GI = PC->GetGameInstance())
        {
            if (const UIngredientRegistrySubsystem* Registry = GI->GetSubsystem<UIngredientRegistrySubsystem>())
            {
                if (const FIngredientTableRow* Row = Registry->GetRow(HeldItem.IngredientID))
                {
                    bMineral = Row->Class == EIngredientClass::Mineral;
                    bLiquid = bLiquid || Row->Class == EIngredientClass::Water;
                }
            }
        }
    }
    HeldActor->ShowItem(HeldItem, bMineral, bLiquid);
}

void UHeldItemComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
    if (!bHolding) return;

    // Предмет ушёл из котомки (потратили, отдали) -- рука пустеет. Иначе
    // снимок обновляется: предмет в руке стареет вместе с тем, что в котомке.
    const int32 Index = ResolveHeldIndex();
    if (Index == INDEX_NONE)
    {
        PutAway();
        return;
    }
    if (const AHerbalistPlayerController* Owner = Cast<AHerbalistPlayerController>(GetOwner()))
    {
        HeldItem = Owner->InventoryComponent->GetItems()[Index];
        HeldIndexHint = Index;
    }

    APlayerController* PC = Cast<APlayerController>(GetOwner());
    if (!PC || !HeldActor) return;
    FVector ViewLocation;
    FRotator ViewRotation;
    PC->GetPlayerViewPoint(ViewLocation, ViewRotation);
    const FVector Offset = bInspecting ? InspectOffset : HeldOffset;
    HeldActor->SetActorLocation(ViewLocation + ViewRotation.RotateVector(Offset));
    // При осмотре предмет медленно поворачивается -- видно со всех сторон.
    // Своё вращение мышью -- вместе с пестерем (этап 2), когда будет ввод.
    const float Spin = bInspecting ? static_cast<float>(FMath::Fmod(GetWorld()->GetTimeSeconds() * 30.0, 360.0)) : 0.0f;
    HeldActor->SetActorRotation(ViewRotation + FRotator(0.0f, Spin, 0.0f));
}

void UHeldItemComponent::EndPlay(const EEndPlayReason::Type Reason)
{
    PutAway();
    Super::EndPlay(Reason);
}

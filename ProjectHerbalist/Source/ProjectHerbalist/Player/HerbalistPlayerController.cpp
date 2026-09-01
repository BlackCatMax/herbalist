// HerbalistPlayerController.cpp
#include "Player/HerbalistPlayerController.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Core/Storage/AlchemyTableActor.h"
#include "Core/Zaryana/MemoryFragmentActor.h"
#include "Core/Interaction/Interactable.h"
#include "Core/Storage/StorageContainer.h"
#include "Core/Subsystems/IngredientRegistrySubsystem.h"
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"
#include "UI/AlchemyTransferWidget.h"
#include "UI/InventoryTransferWidget.h"
#include "UI/InventoryWidget.h"
#include "UI/JournalLogWidget.h"
#include "UI/MemoryRevealWidget.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Dialogue/HerbalistDialogueTypes.h"

// ============================================================================
// ЖИЗНЕННЫЙ ЦИКЛ
// ============================================================================

AHerbalistPlayerController::AHerbalistPlayerController()
{
    InventoryComponent = CreateDefaultSubobject<UHerbalistInventoryComponent>(TEXT("InventoryComponent"));
    JournalComponent = CreateDefaultSubobject<UHerbalistJournalComponent>(TEXT("JournalComponent"));
}

void AHerbalistPlayerController::BeginPlay()
{
    Super::BeginPlay();
    bShowMouseCursor = false;
    CachedWorldManager = FindWorldManager();

    if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
    {
        if (DefaultMappingContext)
        {
            Subsystem->AddMappingContext(DefaultMappingContext, 0);
        }
    }
}

void AHerbalistPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(InputComponent))
    {
        EnhancedInputComponent->BindAction(MoveAction,           ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Move);
        EnhancedInputComponent->BindAction(LookAction,           ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Look);
        EnhancedInputComponent->BindAction(HarvestAction,        ETriggerEvent::Started,   this, &AHerbalistPlayerController::Harvest);
        EnhancedInputComponent->BindAction(InfoAction,           ETriggerEvent::Started,   this, &AHerbalistPlayerController::Info);
        EnhancedInputComponent->BindAction(InventoryAction,      ETriggerEvent::Started,   this, &AHerbalistPlayerController::Inventory);
        // JournalAction — Input Action asset ещё не создан в редакторе (см.
        // комментарий у JournalWidgetClass), проверка на null нужна, пока
        // остальные BindAction её не делают, потому что их actions уже
        // назначены в BP и никогда не бывают nullptr на практике.
        if (JournalAction)
        {
            EnhancedInputComponent->BindAction(JournalAction, ETriggerEvent::Started, this, &AHerbalistPlayerController::Journal);
        }
        EnhancedInputComponent->BindAction(ApplyAlchemyAction,   ETriggerEvent::Started,   this, &AHerbalistPlayerController::ApplyAlchemy);
        EnhancedInputComponent->BindAction(InteractAction,       ETriggerEvent::Started,   this, &AHerbalistPlayerController::Interact);
        EnhancedInputComponent->BindAction(UsePotionAction,      ETriggerEvent::Started,   this, &AHerbalistPlayerController::OnUsePotion);
    }
}

// ============================================================================
// ПЕРЕДВИЖЕНИЕ
// ============================================================================

void AHerbalistPlayerController::Move(const FInputActionValue& Value)
{
    const FVector2D MovementVector = Value.Get<FVector2D>();
    if (APawn* ControlledPawn = GetPawn())
    {
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorForwardVector(), MovementVector.Y);
        ControlledPawn->AddMovementInput(ControlledPawn->GetActorRightVector(),   MovementVector.X);
    }
}

void AHerbalistPlayerController::Look(const FInputActionValue& Value)
{
    const FVector2D LookVector = Value.Get<FVector2D>();
    AddYawInput(LookVector.X);
    AddPitchInput(LookVector.Y);
}

// ============================================================================
// ВСПОМОГАТЕЛЬНЫЕ
// ============================================================================

AGridWorldManager* AHerbalistPlayerController::FindWorldManager() const
{
    if (CachedWorldManager)
    {
        return CachedWorldManager;
    }

    for (TActorIterator<AGridWorldManager> It(GetWorld()); It; ++It)
    {
        return *It;
    }
    return nullptr;
}

void AHerbalistPlayerController::GetCellFromHit(const FHitResult& Hit, int32& OutX, int32& OutY) const
{
    OutX = -1;
    OutY = -1;

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    WorldManager->WorldPositionToCell(Hit.Location, OutX, OutY);
}

void AHerbalistPlayerController::UpdateDistortionFromCell(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // ComputePerceptionDistortion (16_Entity_Manifestation) — базовое Distortion
    // клетки плюс ночная надбавка (Морочники) и местные проявления.
    if (WorldManager->GetCell(X, Y))
    {
        CurrentGlobalDistortion = WorldManager->ComputePerceptionDistortion(X, Y);
    }
}

bool AHerbalistPlayerController::GetHitResultFromCamera(FHitResult& OutHit, ECollisionChannel Channel)
{
    FVector CameraLocation;
    FRotator CameraRotation;
    GetPlayerViewPoint(CameraLocation, CameraRotation);

    const FVector End = CameraLocation + CameraRotation.Vector() * 1000.0f;
    FCollisionQueryParams QueryParams;
    QueryParams.AddIgnoredActor(GetPawn());

    const bool bHit = GetWorld()->LineTraceSingleByChannel(OutHit, CameraLocation, End, Channel, QueryParams);

    DrawDebugLine(GetWorld(), CameraLocation, End, bHit ? FColor::Green : FColor::Red, false, 1.0f, 0, 2.0f);
    if (bHit)
    {
        DrawDebugSphere(GetWorld(), OutHit.Location, 10.0f, 12, FColor::Yellow, false, 1.0f);
    }
    return bHit;
}

bool AHerbalistPlayerController::CanHarvestActor(AActor* TargetActor) const
{
    if (!TargetActor || !GetPawn()) return false;
    const float Distance = FVector::Dist(GetPawn()->GetActorLocation(), TargetActor->GetActorLocation());
    return Distance <= MaxHarvestDistance;
}

// ============================================================================
// СБОР УРОЖАЯ
// ============================================================================

void AHerbalistPlayerController::Harvest()
{
    // GetHitResultUnderCursor читает позицию МЫШИ -- бессмысленно от первого
    // лица (bShowMouseCursor=false в BeginPlay, курсор не двигается вместе с
    // камерой). Каждый другой интерактивный обработчик в этом файле (Info/
    // Interact/ApplyAlchemy/UsePotion) уже использует GetHitResultFromCamera
    // -- сбор урожая, главный игровой глагол, был единственным исключением.
    // Второй канал (ECC_GameTraceChannel1) сохранён как и был -- отдельный
    // trace channel специально для собираемых ресурсов, не блокирующих
    // обычную видимость.
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit, ECC_Visibility))
    {
        if (!GetHitResultFromCamera(Hit, ECC_GameTraceChannel1))
        {
            return;
        }
    }

    // Сбор актора ресурса
    if (AHerbalistResourceActor* Resource = Cast<AHerbalistResourceActor>(Hit.GetActor()))
    {
        TryHarvestResource(Resource);
        return;
    }

    // Сбор воды (без актора)
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (GetPawn())
    {
        const float Dist = FVector::Dist(GetPawn()->GetActorLocation(), Hit.Location);
        if (Dist > MaxHarvestDistance) return;
    }

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    FGridCell* Cell = WorldManager->GetCell(X, Y);
    if (!Cell || !Cell->bIsWater) return;

    WorldManager->CollectWater(X, Y);
    UE_LOG(LogHerbalistPlayer, Log, TEXT("Collected water from cell (%d,%d)"), X, Y);
}

bool AHerbalistPlayerController::TryHarvestResource(AHerbalistResourceActor* Resource)
{
    if (!Resource) return false;

    if (!CanHarvestActor(Resource))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Too far to harvest %s"), *Resource->GetName());
        return false;
    }

    if (Resource->IsBeingHarvested())
    {
        UE_LOG(LogHerbalistPlayer, Verbose, TEXT("%s is already being harvested"), *Resource->GetName());
        return false;
    }

    Resource->Harvest();
    return true;
}

// ============================================================================
// ИНФОРМАЦИЯ / ПРАВЫЙ КЛИК
// ============================================================================

void AHerbalistPlayerController::Info()
{
    OnRightClick();
}

void AHerbalistPlayerController::OnRightClick()
{
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    UpdateDistortionFromCell(X, Y);

    AGridWorldManager* WorldManager = FindWorldManager();
    if (WorldManager)
    {
        WorldManager->SelectCell(X, Y);
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Cell info: %s"), *WorldManager->GetSelectedCellInfo());
    }
}

// ============================================================================
// ИНВЕНТАРЬ
// ============================================================================

void AHerbalistPlayerController::Inventory()
{
    if (bIsAnyWidgetOpen && InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    if (bIsAnyWidgetOpen) return;

    if (InventoryWidgetInstance)
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
    }

    if (!InventoryWidgetClass || !InventoryComponent) return;

    InventoryWidgetInstance = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
    if (!InventoryWidgetInstance) return;

    InventoryWidgetInstance->BindInventory(InventoryComponent);
    InventoryWidgetInstance->AddToViewport();

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetIgnoreLookInput(true);
    bIsAnyWidgetOpen = true;
}

// ============================================================================
// АЛХИМИЯ
// ============================================================================

void AHerbalistPlayerController::ApplyAlchemy()
{
    OnApplyAlchemyKey();
}

void AHerbalistPlayerController::OnApplyAlchemyKey()
{
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0) return;

    ApplyTest(X, Y);
}

void AHerbalistPlayerController::OnUsePotion()
{
    UsePotion();
}

void AHerbalistPlayerController::UsePotion()
{
    if (!InventoryComponent) return;

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const TArray<FInventoryItem>& Items = InventoryComponent->GetItems();
    const int32 PotionIndex = Items.IndexOfByPredicate([](const FInventoryItem& Item)
    {
        return Item.IngredientID == FName(TEXT("Potion")) && Item.Count > 0;
    });

    if (PotionIndex == INDEX_NONE) return;

    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0 || !WorldManager->GetCell(X, Y)) return;

    WorldManager->ApplyPotionToCell(X, Y, Items[PotionIndex].State);
    InventoryComponent->RemoveItem(PotionIndex, 1);
}

// ============================================================================
// ВЗАИМОДЕЙСТВИЕ
// ============================================================================

void AHerbalistPlayerController::Interact()
{
    if (CurrentAlchemyWidget && CurrentAlchemyWidget->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit)) return;

    AActor* HitActor = Hit.GetActor();

    // Один интерфейс вместо цепочки Cast<> на каждый интерактивный класс
    // (2026-08-30, "заводим родительские классы для сущностей и связки") —
    // см. Core/Interaction/Interactable.h. AHerbalistResourceActor::Harvest()
    // намеренно не через этот путь, см. комментарий там же.
    if (HitActor && HitActor->Implements<UInteractable>())
    {
        IInteractable::Execute_OnInteract(HitActor, this);
    }
}

// ============================================================================
// ЗАКРЫТИЕ ВИДЖЕТОВ
// ============================================================================

void AHerbalistPlayerController::CloseAnyWidget()
{
    if (InventoryWidgetInstance && InventoryWidgetInstance->IsInViewport())
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
    }

    if (JournalWidgetInstance && JournalWidgetInstance->IsInViewport())
    {
        JournalWidgetInstance->RemoveFromParent();
        JournalWidgetInstance = nullptr;
    }

    if (CurrentAlchemyWidget && CurrentAlchemyWidget->IsInViewport())
    {
        CurrentAlchemyWidget->RemoveFromParent();
        CurrentAlchemyWidget = nullptr;
        CurrentAlchemyTable = nullptr;
    }

    if (CurrentTransferWidget && CurrentTransferWidget->IsInViewport())
    {
        CurrentTransferWidget->RemoveFromParent();
        CurrentTransferWidget = nullptr;
    }

    bShowMouseCursor = false;
    FInputModeGameOnly GameMode;
    SetInputMode(GameMode);
    SetIgnoreLookInput(false);
    bIsAnyWidgetOpen = false;
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ (СТАРЫЕ)
// ============================================================================

void AHerbalistPlayerController::HarvestTest(int32 X, int32 Y)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->HarvestTest(X, Y);
    }
}

void AHerbalistPlayerController::ApplyTest(int32 X, int32 Y)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->ApplyTest(X, Y);
    }
}

void AHerbalistPlayerController::ShowInventory()
{
    Inventory();
}

void AHerbalistPlayerController::SetGatheringTool(FString ToolName)
{
    ToolName.ToLowerInline();
    if (ToolName == TEXT("iron"))         CurrentGatheringTool = EGatheringTool::IronBlade;
    else if (ToolName == TEXT("copper"))  CurrentGatheringTool = EGatheringTool::CopperBlade;
    else if (ToolName == TEXT("bone"))    CurrentGatheringTool = EGatheringTool::BoneKnife;
    else if (ToolName == TEXT("hands"))   CurrentGatheringTool = EGatheringTool::BareHands;
    else
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("SetGatheringTool: unknown tool '%s' (ожидались hands/iron/copper/bone)"), *ToolName);
        return;
    }
    UE_LOG(LogHerbalistPlayer, Log, TEXT("SetGatheringTool: %s"), *ToolName);
}

namespace
{
    void PrintDialogueNode(const FDialogueDefinition& Def, const FDialogueNode& Node, float Respect)
    {
        UE_LOG(LogHerbalistPlayer, Log, TEXT("[Talk:%s] %s"), *Def.DialogueID.ToString(), *Node.SpeakerLine.ToString());
        const TArray<const FDialogueBranch*> Available = GetAvailableBranches(Node, Respect);
        if (Available.Num() == 0)
        {
            UE_LOG(LogHerbalistPlayer, Log, TEXT("[Talk] (нечего ответить -- разговор окончен)"));
            return;
        }
        for (int32 i = 0; i < Available.Num(); ++i)
        {
            UE_LOG(LogHerbalistPlayer, Log, TEXT("[Talk] %d) %s"), i, *Available[i]->ActionText.ToString());
        }
    }
}

void AHerbalistPlayerController::TalkTo(int32 X, int32 Y)
{
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TalkTo: no world manager"));
        return;
    }

    const FEntityLandmark* Landmark = Grid->FindLandmarkAt(FIntPoint(X, Y));
    if (!Landmark)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TalkTo: no one to talk to at (%d,%d)"), X, Y);
        return;
    }

    const FDialogueDefinition* Def = FindDialogueDefinition(Landmark->EntityID);
    if (!Def)
    {
        // Честный пробел, не крах: у большинства хозяев места пока нет
        // записанного дерева (см. комментарий у GetDialogueDefinitions,
        // "Домовой -- первый и пока единственный пример").
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TalkTo: %s has no dialogue tree yet"), *Landmark->EntityID.ToString());
        return;
    }

    // Реанимированный примитив (§ комментарий у ECommandPrimitive::Talk,
    // CommandTypes.h) -- инертен в ExecutePipeline, ставится в очередь ради
    // архитектурной полноты Q/T/D/S/B, не ради эффекта.
    FCommandEntry Cmd;
    Cmd.Primitive = ECommandPrimitive::Talk;
    Cmd.Talk.DialogueID = Def->DialogueID;
    Grid->QueueCommand(Cmd);

    CurrentDialogueID = Def->DialogueID;
    CurrentDialogueNodeID = Def->StartNodeID;
    CurrentDialogueCell = FIntPoint(X, Y);

    const FDialogueNode* Node = FindDialogueNode(*Def, CurrentDialogueNodeID);
    if (Node)
    {
        PrintDialogueNode(*Def, *Node, Landmark->Respect);
    }
}

void AHerbalistPlayerController::ChooseDialogueBranch(int32 BranchIndex)
{
    if (CurrentDialogueID.IsNone())
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ChooseDialogueBranch: no active conversation (call TalkTo first)"));
        return;
    }

    AGridWorldManager* Grid = FindWorldManager();
    const FEntityLandmark* Landmark = Grid ? Grid->FindLandmarkAt(CurrentDialogueCell) : nullptr;
    const FDialogueDefinition* Def = FindDialogueDefinition(CurrentDialogueID);
    const FDialogueNode* Node = Def ? FindDialogueNode(*Def, CurrentDialogueNodeID) : nullptr;
    if (!Landmark || !Def || !Node)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ChooseDialogueBranch: conversation partner is gone"));
        CurrentDialogueID = NAME_None;
        return;
    }

    const TArray<const FDialogueBranch*> Available = GetAvailableBranches(*Node, Landmark->Respect);
    if (!Available.IsValidIndex(BranchIndex))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ChooseDialogueBranch: %d is not a valid branch (0..%d)"), BranchIndex, Available.Num() - 1);
        return;
    }

    const FName NextNodeID = Available[BranchIndex]->NextNodeID;
    if (NextNodeID.IsNone())
    {
        UE_LOG(LogHerbalistPlayer, Log, TEXT("[Talk:%s] (разговор окончен)"), *CurrentDialogueID.ToString());
        CurrentDialogueID = NAME_None;
        CurrentDialogueNodeID = NAME_None;
        return;
    }

    CurrentDialogueNodeID = NextNodeID;
    const FDialogueNode* NextNode = FindDialogueNode(*Def, CurrentDialogueNodeID);
    if (NextNode)
    {
        PrintDialogueNode(*Def, *NextNode, Landmark->Respect);
    }
}

void AHerbalistPlayerController::OfferToCommunity(FString IngredientList)
{
    if (!InventoryComponent) return;
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    // Тот же приём поиска по имени, что TestNewApply — но по индексу, не по
    // значению: подношение должно списать ровно то, что нашло, а не
    // случайную более позднюю копию с тем же IngredientID.
    TArray<FInventoryItem> Items;
    TArray<int32> Indices;
    TArray<FString> Names;
    IngredientList.ParseIntoArray(Names, TEXT(","), true);
    const TArray<FInventoryItem> CurrentItems = InventoryComponent->GetItems();
    for (const FString& Name : Names)
    {
        const FName IngID(*Name);
        for (int32 i = 0; i < CurrentItems.Num(); ++i)
        {
            if (Indices.Contains(i)) continue;
            if (CurrentItems[i].IngredientID == IngID)
            {
                Items.Add(CurrentItems[i]);
                Indices.Add(i);
                break;
            }
        }
    }

    if (Items.Num() == 0)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("OfferToCommunity: no matching items in inventory"));
        return;
    }

    Grid->OfferToCommunity(Items);

    // Индексы по убыванию — RemoveItem(Index) не должен сдвинуть ещё не
    // обработанные позиции.
    Indices.Sort([](int32 A, int32 B) { return A > B; });
    for (int32 Index : Indices)
    {
        InventoryComponent->RemoveItem(Index, 1);
    }
}

void AHerbalistPlayerController::TradeWithCommunity(FString OfferedIngredientID, FString WantedIngredientID)
{
    if (!InventoryComponent) return;
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    const FName OfferedID(*OfferedIngredientID);
    const TArray<FInventoryItem> CurrentItems = InventoryComponent->GetItems();
    int32 FoundIndex = INDEX_NONE;
    for (int32 i = 0; i < CurrentItems.Num(); ++i)
    {
        if (CurrentItems[i].IngredientID == OfferedID) { FoundIndex = i; break; }
    }
    if (FoundIndex == INDEX_NONE)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TradeWithCommunity: '%s' not found in inventory"), *OfferedIngredientID);
        return;
    }

    FInventoryItem Received;
    if (!Grid->TryTradeWithCommunity(CurrentItems[FoundIndex], FName(*WantedIngredientID), Received))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TradeWithCommunity: trade failed ('%s' unknown to the community, or nothing to offer)"), *WantedIngredientID);
        return;
    }

    // Аудит 2026-08-31: ComputeCommunityTradeValue домножает цену на весь
    // Item.Count предложенного стека (см. GridWorldManagerCommunity.cpp) --
    // значит и списать нужно весь стек, а не 1 единицу. Раньше здесь стояла
    // RemoveItem(FoundIndex, 1): стек из 5 трав оценивался как 5, а терял
    // игрок только 1 -- бесплатная утечка ценности при любом стеке > 1.
    InventoryComponent->RemoveItem(FoundIndex, CurrentItems[FoundIndex].Count);
    InventoryComponent->AddItem(Received, Received.Count);
}

void AHerbalistPlayerController::SetGardenPlot(int32 X, int32 Y, FString NicheName)
{
    AGridWorldManager* Manager = FindWorldManager();
    if (!Manager)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("SetGardenPlot: world manager not found"));
        return;
    }

    NicheName.ToLowerInline();
    EGardenNiche Niche = EGardenNiche::None;
    if (NicheName == TEXT("mycelium"))     Niche = EGardenNiche::Mycelium;
    else if (NicheName == TEXT("cellar"))  Niche = EGardenNiche::RootCellar;
    else if (NicheName == TEXT("pond"))    Niche = EGardenNiche::Pond;
    else if (NicheName == TEXT("sunny"))   Niche = EGardenNiche::SunnyBed;
    else if (NicheName == TEXT("shade"))   Niche = EGardenNiche::ShadeBed;
    else if (NicheName != TEXT("none"))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("SetGardenPlot: unknown niche '%s' (ожидались mycelium/cellar/pond/sunny/shade/none)"), *NicheName);
        return;
    }

    Manager->RegisterGardenPlot(FIntPoint(X, Y), Niche);
}

void AHerbalistPlayerController::FoundBase(int32 X, int32 Y)
{
    AGridWorldManager* Manager = FindWorldManager();
    if (!Manager)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("FoundBase: world manager not found"));
        return;
    }

    Manager->RegisterBase(FIntPoint(X, Y));
}

void AHerbalistPlayerController::GiveZaryanaGifts()
{
    bHasMirror = true;
    bHasYarnBall = true;
    UE_LOG(LogHerbalistPlayer, Log, TEXT("GiveZaryanaGifts: mirror and yarn ball granted"));
}

void AHerbalistPlayerController::UseMirror()
{
    if (!bHasMirror)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseMirror: player does not have the mirror"));
        return;
    }

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const FRealState Perceived = WorldManager->GetZaryanaPerceivedState(MirrorPerceptionRng);
    ShowMemoryRevealText(FText::FromString(FString::Printf(TEXT(
        "Зеркальце показывает Заряну такой, какой её видно отсюда: Purity=%.2f, Corruption=%.2f, Distortion=%.2f."),
        Perceived.Meta.Purity, Perceived.Meta.Corruption, Perceived.Meta.Distortion)));
}

void AHerbalistPlayerController::UseYarnBall(int32 BaseIndex)
{
    if (!bHasYarnBall)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseYarnBall: player does not have the yarn ball"));
        return;
    }

    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const TArray<FHerbalistBase>& Bases = WorldManager->GetBases();
    if (!Bases.IsValidIndex(BaseIndex))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseYarnBall: invalid base index %d (%d bases founded)"), BaseIndex, Bases.Num());
        return;
    }

    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;

    const FVector TargetLocation = WorldManager->GetCellWorldPosition(Bases[BaseIndex].Cell.X, Bases[BaseIndex].Cell.Y);
    const float Distance = FVector::Dist(ControlledPawn->GetActorLocation(), TargetLocation);

    // НЕ мгновенная телепортация (§21.2) — тратит игровое время, соразмерное
    // расстоянию, ДО перемещения пешки: погода/сутки/регенерация видят
    // прошедшее время тем же способом, что и обычный ход Tick(), не задним числом.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float SecondsPerUnit = Settings ? Settings->YarnBallSecondsPerUnit : 0.15f;
    const float ElapsedSeconds = Distance * SecondsPerUnit;
    WorldManager->SetGameClockSeconds(WorldManager->GetGameClockSeconds() + ElapsedSeconds);

    ControlledPawn->SetActorLocationAndRotation(TargetLocation, ControlledPawn->GetActorRotation());
    UE_LOG(LogHerbalistPlayer, Log, TEXT("UseYarnBall: travelled to base %d (%.0f units, +%.1fs game time)"),
        BaseIndex, Distance, ElapsedSeconds);
}

void AHerbalistPlayerController::OfferForArtifact(FString ArtifactID, FString IngredientList)
{
    if (!InventoryComponent) return;
    AGridWorldManager* Manager = FindWorldManager();
    if (!Manager) return;

    // Тот же приём поиска по имени в инвентаре, что уже OfferToCommunity —
    // подношение должно списать ровно то, что нашло, не случайную более
    // позднюю копию с тем же IngredientID.
    TArray<FInventoryItem> Items;
    TArray<int32> Indices;
    TArray<FString> Names;
    IngredientList.ParseIntoArray(Names, TEXT(","), true);
    const TArray<FInventoryItem> CurrentItems = InventoryComponent->GetItems();
    for (const FString& Name : Names)
    {
        const FName IngID(*Name);
        for (int32 i = 0; i < CurrentItems.Num(); ++i)
        {
            if (Indices.Contains(i)) continue;
            if (CurrentItems[i].IngredientID == IngID)
            {
                Items.Add(CurrentItems[i]);
                Indices.Add(i);
                break;
            }
        }
    }

    if (Items.Num() == 0)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("OfferForArtifact: no matching items in inventory"));
        return;
    }

    const FName ArtID(*ArtifactID);
    bool bViaDeception = false;
    const bool bAcquired = Manager->TryAcquireArtifact(ArtID, Items, bViaDeception);
    if (!bAcquired)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("OfferForArtifact: %s not acquired (entity not manifested, already held, or offering too weak)"), *ArtifactID);
        return;
    }

    // Зеркальце/Клубочек — выдают базовый предмет-спутник на честной
    // добыче через общий путь §21.3 (см. ArtifactTypes.h::
    // bWarmsCompanionItem, комментарий у bHasMirror/bHasYarnBall в .h).
    // Прогретое состояние — отдельно, через AGridWorldManager::
    // IsArtifactWarmed, не здесь.
    if (ArtID == FName(TEXT("Зеркальце")))
    {
        bHasMirror = true;
    }
    else if (ArtID == FName(TEXT("Клубочек")))
    {
        bHasYarnBall = true;
    }

    // Индексы по убыванию — RemoveItem(Index) не должен сдвинуть ещё не
    // обработанные позиции (тот же приём, что OfferToCommunity).
    Indices.Sort([](int32 A, int32 B) { return A > B; });
    for (int32 Index : Indices)
    {
        InventoryComponent->RemoveItem(Index, 1);
    }

    UE_LOG(LogHerbalistPlayer, Log, TEXT("OfferForArtifact: %s acquired %s"), *ArtifactID,
        bViaDeception ? TEXT("via deception") : TEXT("honestly"));
}

void AHerbalistPlayerController::UseHorn(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    FText Diagnosis;
    if (!WorldManager->UseHornOnCell(FIntPoint(X, Y), Diagnosis))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseHorn: no Рог, or (%d,%d) is not water"), X, Y);
        return;
    }
    ShowMemoryRevealText(Diagnosis);
}

void AHerbalistPlayerController::UseComb(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->UseCombOnCell(FIntPoint(X, Y)))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseComb: no Гребень, or (%d,%d) is outside the grid"), X, Y);
    }
}

void AHerbalistPlayerController::UseYouthApple()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->UseYouthApple())
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseYouthApple: no Молодильное яблоко"));
    }
}

void AHerbalistPlayerController::UseInvisibilityCap()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->UseInvisibilityCap())
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseInvisibilityCap: no Шапка-невидимка"));
    }
}

void AHerbalistPlayerController::UseLanternDisclosure(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    FText Disclosure;
    if (!WorldManager->UseLanternDisclosureOnCell(FIntPoint(X, Y), Disclosure))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseLanternDisclosure: no Фонарь, not warmed yet, or (%d,%d) is outside the grid"), X, Y);
        return;
    }
    ShowMemoryRevealText(Disclosure);
}

void AHerbalistPlayerController::BecomeBuyanGuardian()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // Успех показывает финальный фрагмент BUYAN_GUARDIAN сам, изнутри
    // TryChooseBuyanPath -> CollectMemoryFragment — не дублирую здесь.
    if (!WorldManager->TryChooseBuyanPath(EBuyanPath::Guardian))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("BecomeBuyanGuardian: not available yet (Буян не достигнут, путь уже выбран, или Clarity/Молва ниже порога)"));
    }
}

void AHerbalistPlayerController::TradePlacesWithZaryana()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // Успех показывает финальный фрагмент BUYAN_TRADE_PLACES сам, изнутри
    // TryChooseBuyanPath -> CollectMemoryFragment — не дублирую здесь.
    if (!WorldManager->TryChooseBuyanPath(EBuyanPath::TradePlaces))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("TradePlacesWithZaryana: not available yet (Буян не достигнут или путь уже выбран)"));
    }
}

void AHerbalistPlayerController::AcceptBuyanReality()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // Успех показывает финальный фрагмент BUYAN_ACCEPT_REALITY сам, изнутри
    // TryChooseBuyanPath -> CollectMemoryFragment — не дублирую здесь.
    if (!WorldManager->TryChooseBuyanPath(EBuyanPath::AcceptReality))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("AcceptBuyanReality: not available yet (Буян не достигнут или путь уже выбран)"));
    }
}

void AHerbalistPlayerController::Journal()
{
    if (bIsAnyWidgetOpen && JournalWidgetInstance && JournalWidgetInstance->IsInViewport())
    {
        CloseAnyWidget();
        return;
    }

    if (bIsAnyWidgetOpen) return;

    if (JournalWidgetInstance)
    {
        JournalWidgetInstance->RemoveFromParent();
        JournalWidgetInstance = nullptr;
    }

    if (!JournalComponent) return;

    // JournalLogWidget не требует JournalWidgetClass/WBP вовсе — строит своё
    // дерево в C++ (см. UI/JournalLogWidget.h). Голый StaticClass() уже
    // достаточно, работает без единого шага в редакторе.
    UJournalLogWidget* LogWidget = CreateWidget<UJournalLogWidget>(GetWorld(), UJournalLogWidget::StaticClass());
    if (!LogWidget) return;

    LogWidget->BindJournal(JournalComponent);
    LogWidget->BindWorldManager(FindWorldManager());
    LogWidget->AddToViewport();
    JournalWidgetInstance = LogWidget;

    bShowMouseCursor = true;
    FInputModeGameAndUI InputMode;
    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(InputMode);
    SetIgnoreLookInput(true);
    bIsAnyWidgetOpen = true;
}

// UMemoryRevealWidget.h -- вызывается извне (AGridWorldManager::
// CollectMemoryFragment/CheckBuyanCondition), не завязан на состояние
// bIsAnyWidgetOpen/CloseAnyWidget() намеренно: попап всплывает поверх
// обычной игры сам по себе, не блокирует ввод и не конкурирует с
// Инвентарём/Травником за тот же канал -- игрок может продолжать двигаться,
// пока текст на экране.
void AHerbalistPlayerController::ShowMemoryRevealText(const FText& Text)
{
    if (!MemoryRevealWidgetInstance)
    {
        MemoryRevealWidgetInstance = CreateWidget<UMemoryRevealWidget>(GetWorld(), UMemoryRevealWidget::StaticClass());
    }
    if (!MemoryRevealWidgetInstance) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float DisplaySeconds = Settings ? Settings->MemoryRevealDisplaySeconds : 7.0f;
    MemoryRevealWidgetInstance->Show(Text, DisplaySeconds);
}

void AHerbalistPlayerController::ToggleJournalUI()
{
    Journal();
}

void AHerbalistPlayerController::SaveGame()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UHerbalistSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UHerbalistSaveSubsystem>())
        {
            SaveSubsystem->SaveGame();
        }
    }
}

void AHerbalistPlayerController::LoadGame()
{
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UHerbalistSaveSubsystem* SaveSubsystem = GI->GetSubsystem<UHerbalistSaveSubsystem>())
        {
            SaveSubsystem->LoadGame();
        }
    }
}

void AHerbalistPlayerController::MassHarvestTest(int32 X, int32 Y, int32 Count)
{
    if (AGridWorldManager* WorldManager = FindWorldManager())
    {
        WorldManager->MassHarvestTest(X, Y, Count);
    }
}

// ============================================================================
// ТЕСТОВЫЕ КОМАНДЫ (НОВЫЙ ПАЙПЛАЙН)
// ============================================================================

void AHerbalistPlayerController::TestNewHarvest(int32 X, int32 Y, FName IngredientID)
{
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive                = ECommandPrimitive::Harvest;
    Cmd.Harvest.TargetCell       = FIntPoint(X, Y);
    Cmd.Harvest.IngredientID     = IngredientID;
    Cmd.Harvest.Amount           = 1;
    Grid->QueueCommand(Cmd);
}

void AHerbalistPlayerController::TestNewTransfer(FName IngredientID, int32 Amount)
{
    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive                     = ECommandPrimitive::Transfer;
    Cmd.Transfer.SourceContainerID    = 0;
    Cmd.Transfer.TargetContainerID    = 1;
    Cmd.Transfer.IngredientID         = IngredientID;
    Cmd.Transfer.Amount               = Amount;
    Grid->QueueCommand(Cmd);
}

void AHerbalistPlayerController::TestNewApply(int32 X, int32 Y, FString IngredientList)
{
    if (!InventoryComponent) return;

    TArray<FInventoryItem> Items;
    TArray<FString> Names;
    IngredientList.ParseIntoArray(Names, TEXT(","), true);
    for (const FString& Name : Names)
    {
        const FName IngID(*Name);
        for (const FInventoryItem& Item : InventoryComponent->GetItems())
        {
            if (Item.IngredientID == IngID)
            {
                Items.Add(Item);
                break;
            }
        }
    }

    if (Items.Num() == 0) return;

    AGridWorldManager* Grid = FindWorldManager();
    if (!Grid) return;

    FCommandEntry Cmd;
    Cmd.Primitive             = ECommandPrimitive::Apply;
    Cmd.Apply.TargetCell      = FIntPoint(X, Y);
    Cmd.Apply.Ingredients     = Items;
    // Coherence считается Pipeline'ом из Ingredients (ComputeIntentCoherence).
    Grid->QueueCommand(Cmd);
}
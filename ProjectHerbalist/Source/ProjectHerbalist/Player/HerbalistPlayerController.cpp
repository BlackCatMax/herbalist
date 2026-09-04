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
        // Незаданный UInputAction -- САМАЯ ТИХАЯ из возможных поломок ввода
        // (2026-09-03, разбор «сбор не работает»). UEnhancedInputComponent::
        // BindAction принимает nullptr без единой жалобы и создаёт привязку,
        // которая просто никогда не сработает: клавиша нажимается, функция
        // не вызывается, в логе пусто. Раньше проверялся только
        // JournalAction, с комментарием «остальные назначены в BP и никогда
        // не бывают nullptr на практике» -- ровно то допущение, которое
        // нечем проверить, когда что-то не работает.
        //
        // Теперь каждая привязка проходит через один хелпер: null называется
        // по имени и Warning'ом, живая -- пишет, к какому ассету привязана.
        // Одного запуска PIE хватает, чтобы отличить «действие не назначено»
        // от «назначено, но клавишу перехватывает другой Mapping Context».
        int32 Bound = 0, Missing = 0;
        auto Bind = [&](UInputAction* Action, const TCHAR* Name, ETriggerEvent Event, void (AHerbalistPlayerController::*Func)())
        {
            if (!Action)
            {
                UE_LOG(LogHerbalistPlayer, Warning, TEXT("Ввод: %s не назначен в Blueprint'е контроллера -- клавиша не будет работать"), Name);
                ++Missing;
                return;
            }
            EnhancedInputComponent->BindAction(Action, Event, this, Func);
            UE_LOG(LogHerbalistPlayer, Log, TEXT("Ввод: %s -> %s"), Name, *Action->GetName());
            ++Bound;
        };

        // Move/Look принимают FInputActionValue, под общий хелпер не идут --
        // у них другая сигнатура обработчика.
        if (MoveAction) { EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Move); ++Bound; }
        else { UE_LOG(LogHerbalistPlayer, Warning, TEXT("Ввод: MoveAction не назначен")); ++Missing; }
        if (LookAction) { EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &AHerbalistPlayerController::Look); ++Bound; }
        else { UE_LOG(LogHerbalistPlayer, Warning, TEXT("Ввод: LookAction не назначен")); ++Missing; }

        Bind(HarvestAction,      TEXT("HarvestAction (сбор)"),      ETriggerEvent::Started, &AHerbalistPlayerController::Harvest);
        Bind(InfoAction,         TEXT("InfoAction"),                ETriggerEvent::Started, &AHerbalistPlayerController::Info);
        Bind(InventoryAction,    TEXT("InventoryAction"),           ETriggerEvent::Started, &AHerbalistPlayerController::Inventory);
        Bind(JournalAction,      TEXT("JournalAction"),             ETriggerEvent::Started, &AHerbalistPlayerController::Journal);
        Bind(ApplyAlchemyAction, TEXT("ApplyAlchemyAction"),        ETriggerEvent::Started, &AHerbalistPlayerController::ApplyAlchemy);
        Bind(InteractAction,     TEXT("InteractAction"),            ETriggerEvent::Started, &AHerbalistPlayerController::Interact);
        Bind(UsePotionAction,    TEXT("UsePotionAction"),           ETriggerEvent::Started, &AHerbalistPlayerController::OnUsePotion);

        UE_LOG(LogHerbalistPlayer, Log, TEXT("Ввод: привязано %d действий, не назначено %d"), Bound, Missing);
    }
    else
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Ввод: InputComponent не UEnhancedInputComponent -- ни одно действие не привязано"));
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

void AHerbalistPlayerController::AddArtifactToInventory(FName ArtifactOrFeatherID)
{
    if (!InventoryComponent) return;

    FInventoryItem Item;
    Item.IngredientID = ArtifactOrFeatherID;
    Item.Count = 1;
    // Артефакты/перья не портятся, в отличие от собранных трав -- та же
    // защита, что уже DecayRate=0 на стороне DT_IngredientClass (оба слоя,
    // не один: DecayRate защищает саму формулу порчи, этот флаг защищает
    // даже путь, который DecayRate не читает).
    Item.bSubjectToDecay = false;
    InventoryComponent->AddItem(Item);
}

bool AHerbalistPlayerController::RemoveArtifactFromInventory(FName ArtifactOrFeatherID)
{
    if (!InventoryComponent) return false;

    const TArray<FInventoryItem> Items = InventoryComponent->GetItems();
    for (int32 i = 0; i < Items.Num(); ++i)
    {
        if (Items[i].IngredientID == ArtifactOrFeatherID)
        {
            return InventoryComponent->RemoveItem(i, 1);
        }
    }
    return false;
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

void AHerbalistPlayerController::HarvestHere()
{
    UE_LOG(LogHerbalistPlayer, Log, TEXT("HarvestHere: вызов сбора из консоли, ввод не участвует"));
    Harvest();
}

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
    // Сбор — главный глагол игры, и до 2026-09-03 он был единственным
    // действием, которое при неудаче не говорило НИЧЕГО: ни луч мимо, ни
    // «под прицелом камень», ни «в клетке пусто» не попадали в лог. Разбор
    // PIE-сессии пользователя упёрся ровно в это молчание. Теперь у каждого
    // выхода есть своя причина в логе (Log, не Warning: промах по цели —
    // нормальная часть игры, а не ошибка).
    FHitResult Hit;
    if (!GetHitResultFromCamera(Hit, ECC_Visibility))
    {
        if (!GetHitResultFromCamera(Hit, ECC_GameTraceChannel1))
        {
            UE_LOG(LogHerbalistPlayer, Log, TEXT("Сбор: луч не встретил ничего (ни ECC_Visibility, ни канал ресурсов) на 1000 см вперёд"));
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
        if (Dist > MaxHarvestDistance)
        {
            UE_LOG(LogHerbalistPlayer, Log, TEXT("Сбор: до точки %.0f см, предел %.0f см -- подойди ближе"), Dist, MaxHarvestDistance);
            return;
        }
    }

    int32 X, Y;
    GetCellFromHit(Hit, X, Y);
    if (X < 0)
    {
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Сбор: точка попадания вне сетки"));
        return;
    }

    FGridCell* Cell = WorldManager->GetCell(X, Y);
    if (!Cell || !Cell->bIsWater)
    {
        // Самый частый и самый непонятный случай: игрок целится в землю, на
        // которой ничего не выросло. Причин ровно две, и обе стоит назвать,
        // иначе отличить «не туда смотрю» от «мир пуст» невозможно.
        const int32 Here = Cell ? Cell->ResourceActors.Num() : 0;
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Сбор: под прицелом %s -- не ресурс и не вода. Клетка (%d,%d): ресурсов %d%s"),
            *GetNameSafe(Hit.GetActor()), X, Y, Here,
            (Cell && Here == 0 && !WorldManager->IsCellClaimedByBiomeRegion(*Cell))
                ? TEXT(" (клетка вне всех ABiomeRegionVolume -- контент тут не спавнится вовсе)")
                : TEXT(""));
        return;
    }

    WorldManager->CollectWater(X, Y);
    UE_LOG(LogHerbalistPlayer, Log, TEXT("Collected water from cell (%d,%d)"), X, Y);
}

bool AHerbalistPlayerController::TryHarvestResource(AHerbalistResourceActor* Resource)
{
    if (!Resource) return false;

    if (!CanHarvestActor(Resource))
    {
        // Числа, а не просто "далеко": предел меряется от ЦЕНТРА КАПСУЛЫ
        // пешки (примерно на уровне пояса) до НАЧАЛА КООРДИНАТ растения (на
        // земле), поэтому даже стоя вплотную набегает почти метр по
        // вертикали. Без цифр в логе понять, что упираешься именно в
        // MaxHarvestDistance, а не в промах прицела, невозможно.
        const float Dist = GetPawn() ? FVector::Dist(GetPawn()->GetActorLocation(), Resource->GetActorLocation()) : -1.0f;
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Сбор: %s далеко -- %.0f см при пределе MaxHarvestDistance=%.0f см"),
            *Resource->GetName(), Dist, MaxHarvestDistance);
        return false;
    }

    if (Resource->IsBeingHarvested())
    {
        UE_LOG(LogHerbalistPlayer, Verbose, TEXT("%s is already being harvested"), *Resource->GetName());
        return false;
    }

    // Успех тоже обязан быть слышен. Иначе "не смог собрать" неотличимо от
    // "собрал, но в инвентарь не доехало": сбор не кладёт предмет напрямую,
    // он ставит команду Harvest в пайплайн, и до инвентаря та доберётся
    // только следующим тиком через SnapshotService -> ApplyStateDelta.
    // Разрыв в этой цепочке выглядел бы снаружи точно так же, как промах.
    Resource->Harvest();
    UE_LOG(LogHerbalistPlayer, Log, TEXT("Сбор: %s -- команда отправлена в пайплайн, предмет придёт в инвентарь следующим тиком"),
        *Resource->GetIngredientID().ToString());
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

    if (bIsAnyWidgetOpen)
    {
        // Открыт ДРУГОЙ виджет (варка, диалог). Снаружи это неотличимо от
        // «инвентарь сломался»: клавиша нажата, не происходит ничего.
        UE_LOG(LogHerbalistPlayer, Log, TEXT("Инвентарь: уже открыт другой виджет -- закрой его сначала"));
        return;
    }

    if (InventoryWidgetInstance)
    {
        InventoryWidgetInstance->RemoveFromParent();
        InventoryWidgetInstance = nullptr;
    }

    // Обе ссылки назначаются в Blueprint'е контроллера; незаполненная
    // InventoryWidgetClass -- ровно тот случай, когда клавиша молчит, и
    // единственный способ это увидеть был раньше — читать код.
    if (!InventoryWidgetClass)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Инвентарь: InventoryWidgetClass не задан в Blueprint'е контроллера -- открывать нечего"));
        return;
    }
    if (!InventoryComponent)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Инвентарь: InventoryComponent отсутствует на контроллере"));
        return;
    }

    InventoryWidgetInstance = CreateWidget<UInventoryWidget>(GetWorld(), InventoryWidgetClass);
    if (!InventoryWidgetInstance)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("Инвентарь: CreateWidget вернул null для класса %s"), *GetNameSafe(InventoryWidgetClass));
        return;
    }

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
    else if (NicheName == TEXT("cave"))    Niche = EGardenNiche::Cave;
    else if (NicheName != TEXT("none"))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("SetGardenPlot: unknown niche '%s' (ожидались mycelium/cellar/pond/sunny/shade/cave/none)"), *NicheName);
        return;
    }

    Manager->RegisterGardenPlot(FIntPoint(X, Y), Niche);
}

void AHerbalistPlayerController::ActivateWard(FString CrystalIngredientID)
{
    if (!InventoryComponent)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ActivateWard: no inventory component"));
        return;
    }
    AGridWorldManager* Manager = FindWorldManager();
    if (!Manager)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ActivateWard: world manager not found"));
        return;
    }

    // Владение — тот же поиск по имени, что уже OfferToCommunity, но БЕЗ
    // списания: оберег не расходуется активацией (см. комментарий у
    // объявления). Достаточно хотя бы одной единицы в инвентаре в момент
    // активации.
    const FName CrystalID(*CrystalIngredientID);
    bool bHasCrystal = false;
    for (const FInventoryItem& Item : InventoryComponent->GetItems())
    {
        if (Item.IngredientID == CrystalID && Item.Count > 0)
        {
            bHasCrystal = true;
            break;
        }
    }
    if (!bHasCrystal)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ActivateWard: no '%s' in inventory"), *CrystalIngredientID);
        return;
    }

    // Резолв WardEffectType — через IngredientRegistrySubsystem, тот же
    // приём, что уже AGridWorldManager::SpawnResourceActor. НЕ покрыто
    // автотестом на этом уровне: GameInstanceSubsystem недоступен в
    // Editor-мире автотестов, тот же класс пробела, что уже у
    // TradeWithCommunity (см. ROADMAP.md) — сам эффект (AGridWorldManager::
    // ActivateWardBrewBoost/ActivateWardConcealment/IsWard*Active) протестирован
    // напрямую, минуя этот резолв.
    UGameInstance* GameInstance = GetGameInstance();
    UIngredientRegistrySubsystem* IngredientSubsystem = GameInstance ? GameInstance->GetSubsystem<UIngredientRegistrySubsystem>() : nullptr;
    const FIngredientTableRow* Row = IngredientSubsystem ? IngredientSubsystem->GetRow(CrystalID) : nullptr;
    if (!Row || !Row->bIsWard || Row->WardEffectType == EWardEffectType::None)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("ActivateWard: '%s' is not a ward"), *CrystalIngredientID);
        return;
    }

    switch (Row->WardEffectType)
    {
    case EWardEffectType::BrewBoost:
        Manager->ActivateWardBrewBoost();
        break;
    case EWardEffectType::EntityConceal:
    {
        // Настоящая зона (тот же приём, что уже UseInvisibilityCap) — центр
        // берётся от клетки, где стоит игрок в момент активации.
        APawn* ControlledPawn = GetPawn();
        int32 X = 0, Y = 0;
        if (!ControlledPawn || !Manager->WorldPositionToCell(ControlledPawn->GetActorLocation(), X, Y))
        {
            UE_LOG(LogHerbalistPlayer, Warning, TEXT("ActivateWard: no pawn, or pawn is outside the grid"));
            return;
        }
        Manager->ActivateWardConcealment(FIntPoint(X, Y));
        break;
    }
    default:
        break;
    }
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

    // "Усиленное Зеркальце — иногда пророческое" (21_Journey_And_Artifacts.md
    // §21.4) — раньше только направление, не реализовано (Warmth не давала
    // Зеркальцу никакого эффекта вовсе). Реализовано здесь 2026-09-02 как
    // прямое условие для Пера Гамаюна (16_Entity_Manifestation.md §16.4):
    // прогретое Зеркальце даёт вероятностный шанс честного (без шума
    // PerceiveRealState) чтения вместо обычного зашумлённого; съеденное
    // Перо Гамаюна закрепляет этот шанс как гарантированный навсегда
    // (AGridWorldManager::IsGamayunPropheticGuaranteed).
    static const FName MirrorID(TEXT("Зеркальце"));
    bool bProphetic = false;
    if (WorldManager->IsArtifactWarmed(MirrorID))
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        const float Chance = WorldManager->IsGamayunPropheticGuaranteed()
            ? 1.0f : (Settings ? Settings->MirrorPropheticChance : 0.3f);
        bProphetic = MirrorPerceptionRng.FRand() < Chance;
    }

    const FRealState Result = bProphetic
        ? WorldManager->GetZaryanaTrueState()
        : WorldManager->GetZaryanaPerceivedState(MirrorPerceptionRng);

    ShowMemoryRevealText(FText::FromString(FString::Printf(TEXT(
        "Зеркальце показывает Заряну %s: Purity=%.2f, Corruption=%.2f, Distortion=%.2f."),
        bProphetic ? TEXT("такой, какая она есть на самом деле -- ясно, без сомнений") : TEXT("такой, какой её видно отсюда"),
        Result.Meta.Purity, Result.Meta.Corruption, Result.Meta.Distortion)));
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

    // Настоящее инвентарное представление (2026-09-02) — см. комментарий
    // у AddArtifactToInventory в шапке .h.
    AddArtifactToInventory(ArtID);

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

void AHerbalistPlayerController::LureSwampTsar(int32 X, int32 Y, FString PotionIngredientID)
{
    if (!InventoryComponent) return;
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const FName ID(*PotionIngredientID);
    const TArray<FInventoryItem> CurrentItems = InventoryComponent->GetItems();
    int32 FoundIndex = INDEX_NONE;
    for (int32 i = 0; i < CurrentItems.Num(); ++i)
    {
        if (CurrentItems[i].IngredientID == ID)
        {
            FoundIndex = i;
            break;
        }
    }
    if (FoundIndex == INDEX_NONE)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("LureSwampTsar: no %s in inventory"), *PotionIngredientID);
        return;
    }

    bool bGranted = false;
    const bool bAttempted = WorldManager->TryLureSwampTsarWithPotion(FIntPoint(X, Y), CurrentItems[FoundIndex].State, bGranted);
    if (!bAttempted)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("LureSwampTsar: Болотный царь not manifested near (%d,%d), or Фонарь already held"), X, Y);
        return;
    }

    // Приманка расходуется в любом случае -- удалась она или нет (тот же
    // приём индексов по убыванию не нужен, здесь ровно один предмет).
    InventoryComponent->RemoveItem(FoundIndex, 1);

    if (bGranted)
    {
        AddArtifactToInventory(FName(TEXT("Фонарь")));
    }

    ShowMemoryRevealText(bGranted
        ? FText::FromString(TEXT("Пока Болотный царь таращится на подделку, вы хватаете настоящий Фонарь и уходите в туман."))
        : FText::FromString(TEXT("Царь на миг замирает над ложным зельем -- и тут же снова смотрит на вас. Не в этот раз.")));

    UE_LOG(LogHerbalistPlayer, Log, TEXT("LureSwampTsar: attempt at (%d,%d), %s"), X, Y, bGranted ? TEXT("Фонарь stolen") : TEXT("failed"));
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
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Гребень")));
}

void AHerbalistPlayerController::UseYouthApple()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->UseYouthApple())
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseYouthApple: no Молодильное яблоко"));
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Молодильное яблоко")));
}

void AHerbalistPlayerController::UseInvisibilityCap()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    // Настоящая зона (2026-09-02) — центр берётся от клетки, где стоит
    // игрок в момент применения, тот же приём определения клетки, что уже
    // AAlchemyTableActor::BeginPlay использует для капища (WorldPositionToCell).
    APawn* ControlledPawn = GetPawn();
    int32 X = 0, Y = 0;
    if (!ControlledPawn || !WorldManager->WorldPositionToCell(ControlledPawn->GetActorLocation(), X, Y))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseInvisibilityCap: no pawn, or pawn is outside the grid"));
        return;
    }

    if (!WorldManager->UseInvisibilityCap(FIntPoint(X, Y)))
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

void AHerbalistPlayerController::AcquireFeather(FString FeatherID)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const FName ID(*FeatherID);
    if (!WorldManager->TryAcquireProphetFeather(ID))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("AcquireFeather: %s not acquired (trigger not met, already held, or unknown feather)"), *FeatherID);
        return;
    }
    AddArtifactToInventory(ID);
    UE_LOG(LogHerbalistPlayer, Log, TEXT("AcquireFeather: %s acquired"), *FeatherID);
}

void AHerbalistPlayerController::EatGamayunFeather()
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->EatGamayunFeather())
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("EatGamayunFeather: no Перо Гамаюна"));
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Перо Гамаюна")));
    ShowMemoryRevealText(FText::FromString(TEXT("Съеденное перо оставляет во рту вкус ясности. Теперь Зеркальце больше не лжёт -- когда прогрето, оно всегда покажет правду.")));
}

void AHerbalistPlayerController::UseAlkonostFeather(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    const FGridCell* Cell = WorldManager->GetCellConst(X, Y);
    if (!Cell)
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseAlkonostFeather: (%d,%d) is outside the grid"), X, Y);
        return;
    }

    if (!WorldManager->UseAlkonostFeatherOnBiome(Cell->Biome))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseAlkonostFeather: no Перо Алконоста"));
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Перо Алконоста")));
    ShowMemoryRevealText(FText::FromString(TEXT("Песнь Алконоста стелется над всем краем -- на время морок не смеет здесь проявиться.")));
}

void AHerbalistPlayerController::UseSirinFeather(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    FText Disclosure;
    if (!WorldManager->UseSirinFeatherOnCell(FIntPoint(X, Y), Disclosure))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseSirinFeather: no Перо Сирина, or no active Malign spike in that biome"));
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Перо Сирина")));
    ShowMemoryRevealText(Disclosure);
}

void AHerbalistPlayerController::UseZharPtitsaFeather(int32 X, int32 Y)
{
    AGridWorldManager* WorldManager = FindWorldManager();
    if (!WorldManager) return;

    if (!WorldManager->UseZharPtitsaFeatherOnCell(FIntPoint(X, Y)))
    {
        UE_LOG(LogHerbalistPlayer, Warning, TEXT("UseZharPtitsaFeather: no Перо Жар-птицы, or (%d,%d) is outside the grid"), X, Y);
        return;
    }
    RemoveArtifactFromInventory(FName(TEXT("Перо Жар-птицы")));
    ShowMemoryRevealText(FText::FromString(TEXT("Клетка вспыхивает ровным, негаснущим светом -- маленький, вечный отголосок Буяна на карте.")));
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
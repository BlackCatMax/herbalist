// HerbalistSaveSubsystem.cpp
#include "Core/Save/HerbalistSaveSubsystem.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/World/GridWorldManager.h"
#include "Core/World/Trample/TrampleSubsystem.h"
#include "Core/World/Sky/UltraDynamicSkyBridge.h"
#include "Core/BiomeGraph/BiomeGraphSubsystem.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Inventory/HerbalistInventoryComponent.h"
#include "Core/Journal/HerbalistJournalComponent.h"
#include "Player/HerbalistPlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

const FString UHerbalistSaveSubsystem::DefaultSlotName = TEXT("HerbalistSave");

namespace
{
    AGridWorldManager* FindWorldManager(UWorld* World)
    {
        if (!World) return nullptr;
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            return *It;
        }
        return nullptr;
    }
}

bool UHerbalistSaveSubsystem::SaveGame(const FString& SlotName)
{
    const FString Slot = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGridWorldManager* WorldManager = FindWorldManager(World);
    if (!WorldManager)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("SaveGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::CreateSaveGameObject(UHerbalistSaveGame::StaticClass()));
    if (!Save) return false;

    // SaveVersion уже 1 по дефолту UPROPERTY (см. HerbalistSaveTypes.h) —
    // выставляем явно, чтобы будущая смена версии формата была одной
    // видимой строкой здесь, а не тихим переносом дефолта.
    // v2 (2026-09-07): сменилась СЕМАНТИКА BiomeGraphNodes.MorokField/
    // ZaryanaField -- из абсолютных уровней они стали знаковыми отклонениями
    // от природы биома (см. BuildChunkSummary, GridWorldManagerCore.cpp).
    // Формат полей тот же, смысл другой -- ровно тот "случай посложнее
    // простого добавления поля", ради которого версия и заводилась.
    // v3 (2026-09-12, разметка мира, этап 4): основа клетки -- из потоков
    // клеток, у ресурсов появились слоты мест (FSavedCellState::ResourceSlots).
    // v4 (2026-09-13, разметка мира, этап 6): координаты клеток -- глобальные,
    // от начала сетки World Partition.
    // v5 (2026-09-13, разметка мира, этап 8): разметка, в которой записаны
    // координаты клеток; загрузка другой разметки отказывает.
    // v6 (2026-09-13): вода только из регионов воды -- формат прежний, мир
    // другой (довод у предупреждения в LoadGame).
    // v7 (2026-09-14): HomeStorages -- только построенные хранилища; сундуки и
    // станции карты -- PlacedContainers, по имени актора.
    Save->SaveVersion = CurrentSaveVersion;
    Save->RngBaseSeed = WorldManager->RngBaseSeed;
    Save->GridSizeX = WorldManager->GridSizeX;
    Save->GridSizeY = WorldManager->GridSizeY;
    Save->WorldLayout = FWorldLayoutSolver::MakeSavedLayout(WorldManager->ResolvedLayout);
    Save->CurrentTickID = WorldManager->GetCurrentTickID();
    Save->GameClockSeconds = WorldManager->GetGameClockSeconds();
    Save->Cells = WorldManager->CaptureSaveCells();
    Save->EntityLandmarks = WorldManager->GetEntityLandmarks();
    Save->Shrines = WorldManager->GetShrines();
    Save->Molva = WorldManager->Molva;
    Save->GardenPlots = WorldManager->GardenPlots;
    Save->Bases = WorldManager->GetBases();
    Save->AcquiredArtifacts = WorldManager->GetAcquiredArtifacts();
    Save->GlobalPerceptionClarity = WorldManager->GetGlobalPerceptionClarity();
    Save->ClarityAnchor = WorldManager->GetClarityAnchor();
    Save->ClarityResponseSmoothed = WorldManager->GetClarityResponseSmoothed();
    Save->AcquiredFeathers = WorldManager->GetAcquiredFeathers();
    Save->bGamayunPropheticGuaranteed = WorldManager->IsGamayunPropheticGuaranteed();
    Save->bRosaFirstFalseSignalShown = WorldManager->IsRosaFirstFalseSignalShown();
    Save->bBuyanReached = WorldManager->IsBuyanReached();
    Save->ChosenBuyanPath = WorldManager->GetChosenBuyanPath();
    Save->CollectedFragmentIDs = WorldManager->GetCollectedFragmentIDs().Array();
    Save->HomeStorages = WorldManager->CaptureHomeStorages();
    Save->PlacedContainers = WorldManager->CapturePlacedContainers();
    Save->TieredWards = WorldManager->CaptureTieredWards();
    Save->bSilverWardActive = WorldManager->IsSilverWardActive();
    Save->KurganSites = WorldManager->GetKurganSites();

    // Тропы (2026-09-12) живут в мировой подсистеме, не в менеджере.
    if (UTrampleSubsystem* Trample = World ? World->GetSubsystem<UTrampleSubsystem>() : nullptr)
    {
        Save->TrampleChunks = Trample->CaptureSaveChunks();
    }

    // Небо и погода UDS/UDW (2026-09-16) -- тоже мировая подсистема.
    if (UUltraDynamicSkyBridge* SkyBridge = World ? World->GetSubsystem<UUltraDynamicSkyBridge>() : nullptr)
    {
        Save->SkyAndWeatherState = SkyBridge->CaptureState();
    }

    // Точки интереса, §4 (2026-09-06) -- см. довод у полей в HerbalistSaveTypes.h.
    Save->TotemSite = WorldManager->GetTotemSite();
    Save->SvetloyarSite = WorldManager->GetSvetloyarSite();
    Save->GoryuchKamenSite = WorldManager->GetGoryuchKamenSite();
    Save->SoloveySite = WorldManager->GetSoloveySite();
    Save->bSoloveyTriggered = WorldManager->IsSoloveyTriggered();
    Save->bSoloveyCalmed = WorldManager->IsSoloveyCalmed();
    Save->KalinovMostSite = WorldManager->GetKalinovMostSite();

    // Биомный граф (AUDIT_AND_REFACTORING_PLAN.md §7.1, 2026-09-06) —
    // отсутствие графа в мире (тестовое окружение без DA_BiomeGraph) не
    // повод отказывать сохранению целиком, GetNodes() просто пуст.
    if (UBiomeGraphSubsystem* Graph = World ? World->GetSubsystem<UBiomeGraphSubsystem>() : nullptr)
    {
        Save->BiomeGraphNodes = Graph->GetNodes();
    }

    if (AHerbalistPlayerController* PC = World ? Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()) : nullptr)
    {
        if (PC->InventoryComponent)
        {
            Save->InventoryItems = PC->InventoryComponent->GetItems();
            Save->PersonalContainerType = PC->InventoryComponent->ContainerType;
        }
        if (PC->JournalComponent) Save->JournalEntries = PC->JournalComponent->GetEntries();
        Save->bHasMirror = PC->bHasMirror;
        Save->bHasYarnBall = PC->bHasYarnBall;
        if (APawn* Pawn = PC->GetPawn())
        {
            Save->PlayerLocation = Pawn->GetActorLocation();
            Save->PlayerRotation = Pawn->GetActorRotation();
        }
    }

    const bool bSuccess = UGameplayStatics::SaveGameToSlot(Save, Slot, 0);
    UE_LOG(LogHerbalistSave, Log, TEXT("SaveGame: slot '%s' %s (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, bSuccess ? TEXT("OK") : TEXT("FAILED"), Save->Cells.Num(), Save->EntityLandmarks.Num(),
        Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return bSuccess;
}

// Дефолтный Distortion биома по FName -- нужен только миграции v1 -> v2
// ниже (обратного маппинга FName -> EBiomeType в проекте нет намеренно,
// см. BiomeTypes.h). Дублировать общий хелпер ради одного вызова смысла нет.
static float BiomeDefaultDistortionForSave(FName BiomeID)
{
    for (EBiomeType Biome : FBiomeDefaults::GetAllBiomeTypes())
    {
        if (FBiomeDefaults::BiomeTypeToName(Biome) == BiomeID)
        {
            return FBiomeDefaults::GetDefaultState(Biome).Meta.Distortion;
        }
    }
    return 0.0f;
}

void UHerbalistSaveSubsystem::MigrateBiomeGraphNodesV1ToV2(TMap<FName, FBiomeGraphNode>& Nodes)
{
    for (auto& Pair : Nodes)
    {
        const float LegacyAbsoluteMorok = Pair.Value.MorokField;
        Pair.Value.MorokField = LegacyAbsoluteMorok - BiomeDefaultDistortionForSave(Pair.Key);
        Pair.Value.ZaryanaField = 0.0f;
    }
}

bool UHerbalistSaveSubsystem::CheckSaveApplicable(const UHerbalistSaveGame& Save, const AGridWorldManager& Manager, FString& OutReason)
{
    OutReason.Reset();

    // Версия формата (аудит 2026-09-05, см. подробный довод у
    // UHerbalistSaveGame::SaveVersion). Файл НОВЕЕ, чем умеет читать текущий
    // код (например, сохранённый более новой версией игры), отклоняется явно,
    // а не десериализуется вслепую с риском тихо потерять или неверно
    // истолковать поля, которых эта версия ещё не знает.
    if (Save.SaveVersion > CurrentSaveVersion)
    {
        OutReason = FString::Printf(TEXT("SaveVersion %d is newer than this build supports (%d)"), Save.SaveVersion, CurrentSaveVersion);
        return false;
    }

    // Разметка (этап 8, решение пользователя 9): координаты клеток сейва имеют
    // смысл только в той разметке, в которой записаны.
    FString LayoutMismatch;
    if (!FWorldLayoutSolver::IsSaveCompatible(Save.WorldLayout, Manager.ResolvedLayout, LayoutMismatch))
    {
        // Сейв старее v5 разметки не знает вовсе -- причина в версии, а не в
        // карте без ландшафта (ревью этапа 8а).
        OutReason = (!Save.WorldLayout.bValid && Save.SaveVersion < 5)
            ? FString::Printf(TEXT("сейв v%d записан до отпечатка разметки (v5); %s"), Save.SaveVersion, *LayoutMismatch)
            : FString::Printf(TEXT("written for another world layout: %s"), *LayoutMismatch);
        return false;
    }

    // Загрузка не путешествует по уровням -- восстанавливает состояние прямо в
    // текущей живой сессии. Без разметки размер сетки должен совпасть:
    // координаты клеток сохранения не означают ничего на сетке другого
    // размера. С разметкой размер не сверяется: добавленные плитки ландшафта
    // сейв не ломают (решение 14), клетки за убранными отбрасывает
    // ApplySaveCells.
    if (!Manager.ResolvedLayout.bValid && (Manager.GridSizeX != Save.GridSizeX || Manager.GridSizeY != Save.GridSizeY))
    {
        OutReason = FString::Printf(TEXT("grid size mismatch (current %dx%d, saved %dx%d)"),
            Manager.GridSizeX, Manager.GridSizeY, Save.GridSizeX, Save.GridSizeY);
        return false;
    }
    return true;
}

int32 UHerbalistSaveSubsystem::CountSitesOutsideGrid(const AGridWorldManager& Manager)
{
    int32 Count = 0;
    auto CountCell = [&Manager, &Count](const FIntPoint& Cell)
    {
        if (HerbalistCore::IsValidCell(Cell) && !Manager.IsCellInGrid(Cell.X, Cell.Y))
        {
            ++Count;
        }
    };
    for (const FShrine& Shrine : Manager.GetShrines())
    {
        CountCell(Shrine.Cell);
    }
    for (const FEntityLandmark& Landmark : Manager.GetEntityLandmarks())
    {
        CountCell(Landmark.Cell);
    }
    CountCell(Manager.GetTotemSite());
    CountCell(Manager.GetSvetloyarSite());
    CountCell(Manager.GetGoryuchKamenSite());
    CountCell(Manager.GetSoloveySite());
    CountCell(Manager.GetKalinovMostSite());
    return Count;
}

TArray<FIntPoint> UHerbalistSaveSubsystem::CollectSaveSiteCells(const UHerbalistSaveGame& Save, const AGridWorldManager& Manager)
{
    TArray<FIntPoint> Sites;
    for (const FShrine& Shrine : Save.Shrines)
    {
        Sites.Add(Shrine.Cell);
        // Соседи капища за убранными плитками: гашение утечки Морока читает
        // четыре прямые соседние клетки (CollectBorderShrineDamping, ревью
        // 2026-09-13). У капища внутри сетки сосед за её краем -- настоящий край
        // мира: без этой проверки каждая загрузка наращивала бы сетку страницей
        // без земли.
        if (HerbalistCore::IsValidCell(Shrine.Cell) && !Manager.IsCellInGrid(Shrine.Cell.X, Shrine.Cell.Y))
        {
            Sites.Append({ Shrine.Cell + FIntPoint(1, 0), Shrine.Cell - FIntPoint(1, 0), Shrine.Cell + FIntPoint(0, 1), Shrine.Cell - FIntPoint(0, 1) });
        }
    }
    for (const FEntityLandmark& Landmark : Save.EntityLandmarks)
    {
        Sites.Add(Landmark.Cell);
    }
    Sites.Add(Save.TotemSite);
    Sites.Add(Save.SvetloyarSite);
    Sites.Add(Save.GoryuchKamenSite);
    Sites.Add(Save.SoloveySite);
    Sites.Add(Save.KalinovMostSite);
    // Курганов нет намеренно (ревью): разграбление по координате клетку не
    // читает, страница кургану не нужна.
    return Sites;
}

bool UHerbalistSaveSubsystem::LoadGame(const FString& SlotName)
{
    const FString Slot = SlotName.IsEmpty() ? DefaultSlotName : SlotName;
    if (!UGameplayStatics::DoesSaveGameExist(Slot, 0))
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: slot '%s' does not exist"), *Slot);
        return false;
    }

    UHerbalistSaveGame* Save = Cast<UHerbalistSaveGame>(UGameplayStatics::LoadGameFromSlot(Slot, 0));
    if (!Save)
    {
        UE_LOG(LogHerbalistSave, Error, TEXT("LoadGame: slot '%s' failed to deserialize"), *Slot);
        return false;
    }

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    AGridWorldManager* WorldManager = FindWorldManager(World);
    if (!WorldManager)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: no AGridWorldManager in world, aborted"));
        return false;
    }

    // Версия, разметка, размер сетки -- до первой записи в мир: отказ не
    // оставляет частично применённого состояния.
    FString Rejection;
    if (!CheckSaveApplicable(*Save, *WorldManager, Rejection))
    {
        UE_LOG(LogHerbalistSave, Error, TEXT("LoadGame: slot '%s' rejected, nothing applied: %s"), *Slot, *Rejection);
        return false;
    }

    // v3 (2026-09-12, разметка мира, этап 4): тип воды, число, виды и места
    // ресурсов берутся из потоков клеток, и при том же сиде раскладка воды
    // пятнами (тогда ещё была) и всё, что сеялось после неё, другое, чем в сейве
    // старее. v4 (этап 6): координаты клеток глобальные. Сюда сейв старее v5
    // доходит только на карте без разметки (с разметкой он отклонён выше), где
    // начало сетки -- (0, 0): грузится, но клетки и места могут лечь не на тот мир.
    if (Save->SaveVersion < 4)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: slot '%s' has SaveVersion %d, written before per-cell seeds (3): water, sites and resources may not match this world"),
            *Slot, Save->SaveVersion);
    }
    // v6 (2026-09-13): вода только из регионов воды, пятен больше нет. Сейв
    // старее записан в мире с пятнами: bIsWater в дельтах клеток не хранится,
    // поэтому бывшая пятнистая вода загрузится сушей с водным состоянием, а
    // легендарные якоря (в сейв не пишутся) пересеются на другие клетки -- общий
    // WorldRNG пятнами больше не расходуется.
    else if (Save->SaveVersion < 6)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: slot '%s' has SaveVersion %d, written before water-only-from-regions (6): former blob water cells load as land, legendary anchors re-seed elsewhere"),
            *Slot, Save->SaveVersion);
    }
    // v7 (2026-09-14): контейнеры карты сохраняются отдельно. В старом сейве их
    // содержимого нет, а записи о них в HomeStorages RestoreHomeStorages пропускает.
    else if (Save->SaveVersion < 7)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: slot '%s' has SaveVersion %d, written before placed containers (7): chests and stations on the map keep their current contents"),
            *Slot, Save->SaveVersion);
    }

    WorldManager->RngBaseSeed = Save->RngBaseSeed;
    WorldManager->SetCurrentTickID(Save->CurrentTickID);
    WorldManager->SetGameClockSeconds(Save->GameClockSeconds);

    // Тропы -- строго после часов: отсчёт распада восстановленных чанков
    // начинается от часов загруженной сессии.
    if (UTrampleSubsystem* Trample = World ? World->GetSubsystem<UTrampleSubsystem>() : nullptr)
    {
        Trample->RestoreSaveChunks(Save->TrampleChunks);
    }

    // Небо и погода -- после часов: мост ставит время UDS из них. Применяется,
    // как только UDS найден и начал игру (стриминг). Старый сейв без
    // состояния -- погода остаётся текущей.
    if (UUltraDynamicSkyBridge* SkyBridge = World ? World->GetSubsystem<UUltraDynamicSkyBridge>() : nullptr)
    {
        if (!Save->SkyAndWeatherState.IsEmpty())
        {
            SkyBridge->QueueStateForLoad(Save->SkyAndWeatherState, *WorldManager);
        }
    }

    // Аудит 2026-09-05: таймеры оберегов/артефактных эффектов "короткого
    // окна" (Ward*/InvisibilityCap/YouthApple/Alkonost) осознанно не
    // персистятся — но GameClockSeconds выше ТОЛЬКО ЧТО откатился (вперёд
    // или назад, не важно), а сами таймеры без явного сброса остались бы
    // на прежнем значении. Без этого отступление к более раннему сейву
    // могло прочитать давно истёкший/никогда не активированный в этой
    // временной точке оберег как ещё активный на полный WardDurationSeconds
    // заново — см. подробный довод у ResetSessionOnlyWardTimers.
    WorldManager->ResetSessionOnlyWardTimers();

    // Места за убранными плитками ландшафта живут (решение пользователя
    // 2026-09-13): сетка расширяется до их страниц до записи мест и клеток
    // сейва -- иначе ApplySaveCells отбросил бы и клетки самих мест.
    WorldManager->EnsureGridCoversSites(CollectSaveSiteCells(*Save, *WorldManager));

    WorldManager->SetEntityLandmarks(Save->EntityLandmarks);
    WorldManager->SetShrines(Save->Shrines);
    WorldManager->Molva = Save->Molva;
    WorldManager->GardenPlots = Save->GardenPlots;
    WorldManager->SetBases(Save->Bases);
    WorldManager->SetAcquiredArtifacts(Save->AcquiredArtifacts);
    WorldManager->SetGlobalPerceptionClarity(Save->GlobalPerceptionClarity);
    WorldManager->SetClarityAnchor(Save->ClarityAnchor);
    WorldManager->SetClarityResponseSmoothed(Save->ClarityResponseSmoothed);
    WorldManager->SetAcquiredFeathers(Save->AcquiredFeathers);
    WorldManager->SetGamayunPropheticGuaranteed(Save->bGamayunPropheticGuaranteed);
    WorldManager->SetRosaFirstFalseSignalShown(Save->bRosaFirstFalseSignalShown);
    WorldManager->SetBuyanReached(Save->bBuyanReached);
    WorldManager->SetChosenBuyanPath(Save->ChosenBuyanPath);
    WorldManager->SetCollectedFragmentIDs(TSet<FName>(Save->CollectedFragmentIDs));
    WorldManager->ApplySaveCells(Save->Cells);
    WorldManager->RestoreHomeStorages(Save->HomeStorages);
    WorldManager->RestorePlacedContainers(Save->PlacedContainers);
    WorldManager->RestoreTieredWards(Save->TieredWards);
    WorldManager->SetSilverWardActive(Save->bSilverWardActive);
    WorldManager->SetKurganSites(Save->KurganSites);

    // Точки интереса, §4 (2026-09-06) -- сейв без этих полей (старее этого
    // прохода) даёт HerbalistCore::InvalidCell()/false по дефолту UPROPERTY, то же самое,
    // что "точка ещё не сеялась" -- ничего специально не проверяем.
    WorldManager->SetTotemSite(Save->TotemSite);
    WorldManager->SetSvetloyarSite(Save->SvetloyarSite);
    WorldManager->SetGoryuchKamenSite(Save->GoryuchKamenSite);
    WorldManager->SetSoloveySite(Save->SoloveySite);
    WorldManager->SetSoloveyTriggered(Save->bSoloveyTriggered);
    WorldManager->SetSoloveyCalmed(Save->bSoloveyCalmed);
    WorldManager->SetKalinovMostSite(Save->KalinovMostSite);

    // Места за сеткой (ревью этапа 8а). С разметкой сетка выше уже расширена до
    // их страниц (2026-09-13); строка остаётся для карты без разметки, где сетка
    // ручная и место за её краем пропадает.
    const int32 SitesOutsideGrid = CountSitesOutsideGrid(*WorldManager);
    if (SitesOutsideGrid > 0)
    {
        UE_LOG(LogHerbalistSave, Warning, TEXT("LoadGame: %d shrines, entity landmarks or points of interest lie outside the grid and are unreachable (landscape tiles removed?)"), SitesOutsideGrid);
    }

    // Биомный граф (AUDIT_AND_REFACTORING_PLAN.md §7.1, 2026-09-06) —
    // RestoreNodeFieldState сам не трогает узлы, отсутствующие в сейве
    // (например, старый сейв без этого поля вовсе — пустая карта), граф
    // просто остаётся на дефолтах InitializeFromAsset.
    if (UBiomeGraphSubsystem* Graph = World->GetSubsystem<UBiomeGraphSubsystem>())
    {
        // Миграция v1 -> v2 (2026-09-07). В v1 MorokField хранил АБСОЛЮТНЫЙ
        // уровень Морока биома, в v2 -- знаковое ОТКЛОНЕНИЕ от природного
        // Distortion этого биома. Без пересчёта старый сейв Болота (0.70
        // абсолютных) прочитался бы как "+0.70 сверх природных 0.70", то
        // есть мир загрузился бы максимально испорченным -- молча, без
        // единой ошибки в логе.
        //
        // ZaryanaField в v1 был вообще другой величиной (1 - Distortion, то
        // есть зеркало Морока, а не самостоятельная ось), осмысленного
        // соответствия отклонению Stability у него нет -- честнее обнулить
        // (биом стартует со своей природы), чем пересчитывать наугад.
        TMap<FName, FBiomeGraphNode> RestoredNodes = Save->BiomeGraphNodes;
        if (Save->SaveVersion < 2)
        {
            MigrateBiomeGraphNodesV1ToV2(RestoredNodes);
            UE_LOG(LogHerbalistSave, Log, TEXT("LoadGame: сейв v%d -- поля биом-графа пересчитаны в отклонения (v2)"), Save->SaveVersion);
        }
        Graph->RestoreNodeFieldState(RestoredNodes);
    }

    if (AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(World->GetFirstPlayerController()))
    {
        if (PC->InventoryComponent)
        {
            PC->InventoryComponent->RestoreItems(Save->InventoryItems);
            PC->InventoryComponent->ContainerType = Save->PersonalContainerType;
        }
        if (PC->JournalComponent) PC->JournalComponent->RestoreEntries(Save->JournalEntries);
        PC->bHasMirror = Save->bHasMirror;
        PC->bHasYarnBall = Save->bHasYarnBall;
        if (APawn* Pawn = PC->GetPawn())
        {
            Pawn->SetActorLocationAndRotation(Save->PlayerLocation, Save->PlayerRotation);
        }
    }

    UE_LOG(LogHerbalistSave, Log, TEXT("LoadGame: slot '%s' OK (%d cells, %d landmarks, %d inventory items, %d journal entries)"),
        *Slot, Save->Cells.Num(), Save->EntityLandmarks.Num(), Save->InventoryItems.Num(), Save->JournalEntries.Num());
    return true;
}

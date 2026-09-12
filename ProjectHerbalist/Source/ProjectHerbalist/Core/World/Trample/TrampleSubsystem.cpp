// Source/ProjectHerbalist/Core/World/Trample/TrampleSubsystem.cpp

#include "Core/World/Trample/TrampleSubsystem.h"

#include "Core/Config/HerbalistSettings.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/World/GridWorldManager.h"
#include "HerbalistLogChannels.h"

#include "Camera/PlayerCameraManager.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Materials/MaterialParameterCollection.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"

bool UTrampleSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
    return WorldType == EWorldType::Game || WorldType == EWorldType::PIE || WorldType == EWorldType::Editor;
}

TStatId UTrampleSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UTrampleSubsystem, STATGROUP_Tickables);
}

void UTrampleSubsystem::Deinitialize()
{
    ResetTrample();
    Super::Deinitialize();
}

void UTrampleSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);

    UWorld* World = GetWorld();
    if (!World || !World->IsGameWorld())
    {
        return;
    }

    FVector Viewer;
    if (APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0))
    {
        // Радиус -- реальная ширина тела пешки (у персонажа это капсула), не
        // выдуманная константа.
        bool bOnGround = true;
        if (const ACharacter* Character = Cast<ACharacter>(Pawn))
        {
            const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
            bOnGround = Movement && Movement->IsMovingOnGround();
        }
        Viewer = Pawn->GetActorLocation();
        FeedWalker(Viewer, Pawn->GetSimpleCollisionRadius(), bOnGround);
    }
    else if (const APlayerCameraManager* Camera = UGameplayStatics::GetPlayerCameraManager(World, 0))
    {
        LastWalkerXY.Reset();
        Viewer = Camera->GetCameraLocation();
    }
    else
    {
        LastWalkerXY.Reset();
        return;
    }

    UpdateDisplay(Viewer, DeltaTime);
}

void UTrampleSubsystem::FeedWalker(const FVector& Location, float RadiusCm, bool bOnGround)
{
    const FVector2D XY(Location);
    if (!bOnGround || RadiusCm <= 0.0f)
    {
        LastWalkerXY.Reset();
        return;
    }
    if (!LastWalkerXY.IsSet())
    {
        LastWalkerXY = XY;
        return;
    }

    const double Distance = FVector2D::Distance(LastWalkerXY.GetValue(), XY);

    // Скачок дальше чанка за один кадр -- телепорт или загрузка, а не шаги.
    if (Distance > FTrampleField::ChunkSizeCm)
    {
        LastWalkerXY = XY;
        return;
    }
    if (Distance < MinStrokeDistanceCm)
    {
        return;
    }

    AddStroke(LastWalkerXY.GetValue(), XY, RadiusCm);
    LastWalkerXY = XY;
}

void UTrampleSubsystem::AddStroke(const FVector2D& From, const FVector2D& To, float RadiusCm)
{
    Field.AddStroke(From, To, RadiusCm, GetPassDeposit(), GetNowSeconds(),
        [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); });

    // Рамка штриха в текселях -- её цели пересчитаются в ближайшем кадре.
    FTrampleWindow::FRect Rect;
    Rect.MinGX = FTrampleField::WorldToTexel(FMath::Min(From.X, To.X) - RadiusCm);
    Rect.MinGY = FTrampleField::WorldToTexel(FMath::Min(From.Y, To.Y) - RadiusCm);
    Rect.Width = FTrampleField::WorldToTexel(FMath::Max(From.X, To.X) + RadiusCm) - Rect.MinGX + 1;
    Rect.Height = FTrampleField::WorldToTexel(FMath::Max(From.Y, To.Y) + RadiusCm) - Rect.MinGY + 1;
    PendingStrokeRects.Add(Rect);
}

float UTrampleSubsystem::GetNowSeconds() const
{
    if (ClockOverride.IsSet())
    {
        return ClockOverride.GetValue();
    }
    // Игровые часы менеджера: тропа живёт в игровом времени, как и
    // HarvestStress, и сохраняется вместе с ним. Без менеджера на карте
    // (L_PlaytestPaint) -- время мира.
    if (const AGridWorldManager* Manager = FindManager())
    {
        return Manager->GetGameClockSeconds();
    }
    const UWorld* World = GetWorld();
    return World ? World->GetTimeSeconds() : 0.0f;
}

float UTrampleSubsystem::GetFullClearSeconds(const FIntPoint& ChunkCoord) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float TimeScale = FMath::Max(Settings ? Settings->TrampleTestTimeScale : 1.0f, 0.01f);

    // То же время зарастания, что у HarvestStress клетки под центром чанка:
    // биом, сезон и Лесное капище наследуются сами (болото держит тропу
    // дольше, весна зарастает быстрее). Без менеджера или вне сетки --
    // базовые StressRecoveryGameDays игровых суток.
    if (const AGridWorldManager* Manager = FindManager())
    {
        const FVector2D Center = FTrampleField::GetChunkCenter(ChunkCoord);
        int32 CellX = 0;
        int32 CellY = 0;
        if (Manager->WorldPositionToCell(FVector(Center, 0.0), CellX, CellY))
        {
            if (const FGridCell* Cell = Manager->GetCellConst(CellX, CellY))
            {
                return Manager->GetStressRecoverySecondsForCell(*Cell) / TimeScale;
            }
        }
    }

    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    const float DaySeconds = (Settings ? Settings->GameDayMinutes : 32.0f) * 60.0f;
    return FMath::Max(RecoveryDays * DaySeconds, KINDA_SMALL_NUMBER) / TimeScale;
}

float UTrampleSubsystem::GetPassDeposit() const
{
    // Правило: тропа, по которой проходят раз в игровые сутки, держится на
    // месте -- проход добавляет ровно столько, сколько сутки распада снимают
    // при биоме 1.0: DaySeconds / (RecoveryDays x DaySeconds) = 1/RecoveryDays.
    // Это же число -- порог видимости в FTrampleWindow: то, что держится при
    // проходе раз в сутки, ещё не тропа.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    return 1.0f / FMath::Max(RecoveryDays, 0.01f);
}

float UTrampleSubsystem::GetValueAt(const FVector2D& WorldXY) const
{
    return Field.GetValueAt(WorldXY, GetNowSeconds(),
        [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); });
}

float UTrampleSubsystem::GetDisplayedAt(const FVector2D& WorldXY) const
{
    return Window.GetDisplayedAtTexel(FTrampleField::WorldToTexel(WorldXY.X), FTrampleField::WorldToTexel(WorldXY.Y));
}

float UTrampleSubsystem::GetDecayRefreshIntervalSeconds(float FullClearSeconds)
{
    return FMath::Max(FullClearSeconds, 0.0f) / 255.0f;
}

float UTrampleSubsystem::GetEaseStepSeconds()
{
    return (1.0f / 255.0f) / VisualRatePerSecond;
}

float UTrampleSubsystem::GetFadeStartCm() const
{
    // Начало затухания -- радиус симуляции мира: там же, где стоят ресурсы.
    // Конец -- граница верных данных окна. Стриминг выключен (-1) -- гасим
    // только у самой границы.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Meters = Settings ? Settings->ActiveSimulationRadiusMeters : -1.0f;
    if (Meters < 0.0f)
    {
        return GetFadeEndCm();
    }
    return FMath::Min(Meters * 100.0f, GetFadeEndCm());
}

AGridWorldManager* UTrampleSubsystem::FindManager() const
{
    if (AGridWorldManager* Cached = CachedManager.Get())
    {
        return Cached;
    }
    if (UWorld* World = GetWorld())
    {
        for (TActorIterator<AGridWorldManager> It(World); It; ++It)
        {
            CachedManager = *It;
            return *It;
        }
    }
    return nullptr;
}

void UTrampleSubsystem::RefreshRect(const FTrampleWindow::FRect& Rect, float NowSeconds, bool bSnap)
{
    const FTrampleWindow::FRect Clipped = FTrampleWindow::FRect::Intersect(Rect, Window.GetWindowRect());
    if (Clipped.IsEmpty())
    {
        return;
    }
    TArray<float> Values;
    Field.SampleTexelRect(Clipped.MinGX, Clipped.MinGY, Clipped.Width, Clipped.Height, NowSeconds,
        [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); }, Values);
    Window.SetTargets(Clipped, Values, GetPassDeposit(), bSnap);
}

void UTrampleSubsystem::UpdateDisplay(const FVector& ViewerLocation, float DeltaSeconds)
{
    const float Now = GetNowSeconds();
    const FVector2D ViewerXY(ViewerLocation);

    // 1. Окно. Вошедшие полосы получают значения сразу: они входят у края,
    // в зоне затухания, скачок там не виден.
    for (const FTrampleWindow::FRect& Entered : Window.Recenter(ViewerXY))
    {
        RefreshRect(Entered, Now, true);
    }

    // 2. Распад -- раз в ступень RGBA8. Пересчитываются только натоптанные
    // чанки и те, что только что опустели (их показ надо увести в ноль).
    const float RefreshInterval = GetDecayRefreshIntervalSeconds(GetFullClearSeconds(FTrampleField::WorldToChunk(ViewerXY)));
    if (LastDecayRefreshSeconds < 0.0f || Now < LastDecayRefreshSeconds || Now - LastDecayRefreshSeconds >= RefreshInterval)
    {
        TArray<FIntPoint> Touched = Field.AdvanceAll(Now, [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); });
        for (const TPair<FIntPoint, FTrampleField::FChunk>& Pair : Field.GetChunks())
        {
            Touched.Add(Pair.Key);
        }
        for (const FIntPoint& Coord : Touched)
        {
            FTrampleWindow::FRect ChunkRect;
            ChunkRect.MinGX = Coord.X * FTrampleField::ChunkTexels;
            ChunkRect.MinGY = Coord.Y * FTrampleField::ChunkTexels;
            ChunkRect.Width = FTrampleField::ChunkTexels;
            ChunkRect.Height = FTrampleField::ChunkTexels;
            RefreshRect(ChunkRect, Now, false);
        }
        LastDecayRefreshSeconds = Now;
    }

    // 3. Штрихи.
    for (const FTrampleWindow::FRect& Rect : PendingStrokeRects)
    {
        RefreshRect(Rect, Now, false);
    }
    PendingStrokeRects.Reset();

    // 4. Догон -- тактами в ступень байта, а не каждый кадр: чаще картинка
    // всё равно не изменится ни на один байт.
    if (Window.HasEasing())
    {
        EaseAccumulatorSeconds += FMath::Max(DeltaSeconds, 0.0f);
        if (EaseAccumulatorSeconds >= GetEaseStepSeconds())
        {
            Window.Ease(VisualRatePerSecond * EaseAccumulatorSeconds);
            EaseAccumulatorSeconds = 0.0f;
        }
    }
    else
    {
        EaseAccumulatorSeconds = 0.0f;
    }

    UploadDirtyTiles();
    PushFrameToCollection(ViewerLocation);
}

void UTrampleSubsystem::UploadDirtyTiles()
{
    if (!MapTarget)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        MapTarget = Settings ? Settings->TrampleMap.LoadSynchronous() : nullptr;
        if (!MapTarget)
        {
            return;   // Текстура не назначена -- поле копится, показа нет.
        }
        if (MapTarget->SizeX != FTrampleWindow::Size || MapTarget->SizeY != FTrampleWindow::Size)
        {
            MapTarget->ResizeTarget(FTrampleWindow::Size, FTrampleWindow::Size);
        }
        // Что лежало в текстуре до нас -- неизвестно.
        Window.MarkAllDirty();
    }
    if (!Window.IsInitialized())
    {
        return;
    }

    FTextureRenderTargetResource* Resource = MapTarget->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    TArray<FIntPoint> Tiles;
    Window.CollectDirtyTiles(Tiles);
    if (Tiles.Num() == 0)
    {
        return;
    }

    struct FTileUpload
    {
        FIntPoint Tile;
        TArray<FColor> Pixels;
    };
    TArray<FTileUpload> Uploads;
    Uploads.Reserve(Tiles.Num());
    for (const FIntPoint& Tile : Tiles)
    {
        FTileUpload& Upload = Uploads.AddDefaulted_GetRef();
        Upload.Tile = Tile;
        Window.BuildTilePixels(Tile, Upload.Pixels);
    }

    // Тот же путь, что у RT_WorldStateMap (GridWorldManagerWorldStateMap.cpp),
    // только по тайлам.
    ENQUEUE_RENDER_COMMAND(HerbalistTrampleMapUpload)(
        [Resource, Uploads = MoveTemp(Uploads)](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* Texture = Resource->GetRenderTargetTexture();
            if (!Texture) return;

            const int32 Tile = FTrampleWindow::TileTexels;
            for (const FTileUpload& Upload : Uploads)
            {
                const FUpdateTextureRegion2D Region(Upload.Tile.X * Tile, Upload.Tile.Y * Tile, 0, 0, Tile, Tile);
                RHICmdList.UpdateTexture2D(Texture, 0, Region, Tile * sizeof(FColor),
                    reinterpret_cast<const uint8*>(Upload.Pixels.GetData()));
            }
        });
}

void UTrampleSubsystem::PushFrameToCollection(const FVector& ViewerLocation)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        return;
    }
    if (!FrameCollection)
    {
        const UHerbalistSettings* Settings = GetHerbalistSettings();
        FrameCollection = Settings ? Settings->TrampleFrameCollection.LoadSynchronous() : nullptr;
        if (!FrameCollection)
        {
            return;
        }
    }

    UKismetMaterialLibrary::SetVectorParameterValue(World, FrameCollection, TEXT("TrampleMapFrame"),
        FLinearColor(FTrampleWindow::WorldSizeCm, GetFadeStartCm(), GetFadeEndCm(), 0.0f));
    UKismetMaterialLibrary::SetVectorParameterValue(World, FrameCollection, TEXT("TramplePlayerPosition"),
        FLinearColor(static_cast<float>(ViewerLocation.X), static_cast<float>(ViewerLocation.Y),
            static_cast<float>(ViewerLocation.Z), 0.0f));
}

TArray<FSavedTrampleChunk> UTrampleSubsystem::CaptureSaveChunks()
{
    // Сохраняем уже распавшиеся значения: при загрузке отсчёт идёт заново
    // от часов загруженной сессии, время между сессиями игровым не является.
    // Показ не сохраняется -- это состояние картинки, а не мира.
    Field.AdvanceAll(GetNowSeconds(), [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); });

    TArray<FSavedTrampleChunk> Result;
    Result.Reserve(Field.GetChunks().Num());
    for (const TPair<FIntPoint, FTrampleField::FChunk>& Pair : Field.GetChunks())
    {
        FSavedTrampleChunk& Saved = Result.AddDefaulted_GetRef();
        Saved.Coord = Pair.Key;
        FTrampleField::QuantizeValues(Pair.Value.Values, Saved.Values);
    }
    return Result;
}

void UTrampleSubsystem::RestoreSaveChunks(const TArray<FSavedTrampleChunk>& InChunks)
{
    // Сброс окна -- следующий кадр заполнит его заново со snap: мир после
    // загрузки не проявляется из нуля у игрока на глазах.
    ResetTrample();

    const float Now = GetNowSeconds();
    for (const FSavedTrampleChunk& Saved : InChunks)
    {
        TArray<float> Values;
        FTrampleField::DequantizeValues(Saved.Values, Values);
        if (!Field.SetChunkValues(Saved.Coord, MoveTemp(Values), Now))
        {
            UE_LOG(LogHerbalistWorld, Warning, TEXT("[Trample] Чанк (%d,%d) в сейве другого размера (%d значений), пропущен"),
                Saved.Coord.X, Saved.Coord.Y, Saved.Values.Num());
        }
    }
}

void UTrampleSubsystem::ResetTrample()
{
    Field.Reset();
    Window.Reset();
    PendingStrokeRects.Reset();
    LastWalkerXY.Reset();
    LastDecayRefreshSeconds = -1.0f;
    EaseAccumulatorSeconds = 0.0f;
}

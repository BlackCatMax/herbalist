// Source/ProjectHerbalist/Core/World/Trample/TrampleSubsystem.cpp

#include "Core/World/Trample/TrampleSubsystem.h"

#include "Core/Config/HerbalistSettings.h"
#include "Core/Save/HerbalistSaveTypes.h"
#include "Core/World/GridWorldManager.h"
#include "HerbalistLogChannels.h"

#include "Components/RuntimeVirtualTextureComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/TextureRenderTarget2D.h"
#include "EngineUtils.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"
#include "VT/RuntimeVirtualTexture.h"
#include "VT/RuntimeVirtualTextureVolume.h"

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

    APawn* Pawn = UGameplayStatics::GetPlayerPawn(World, 0);
    if (!Pawn)
    {
        LastWalkerXY.Reset();
        return;
    }

    // Радиус -- реальная ширина тела пешки (у персонажа это капсула), не
    // выдуманная константа.
    bool bOnGround = true;
    if (const ACharacter* Character = Cast<ACharacter>(Pawn))
    {
        const UCharacterMovementComponent* Movement = Character->GetCharacterMovement();
        bOnGround = Movement && Movement->IsMovingOnGround();
    }

    const FVector Location = Pawn->GetActorLocation();
    FeedWalker(Location, Pawn->GetSimpleCollisionRadius(), bOnGround);
    RefreshDisplays(FVector2D(Location));
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
    // Один проход виден слабо (1/7) и зарастает за сутки; два прохода в сутки
    // протаптывают тропу до полной за неделю -- "за неделю игрового времени".
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float RecoveryDays = Settings ? Settings->StressRecoveryGameDays : 7.0f;
    return 1.0f / FMath::Max(RecoveryDays, 0.01f);
}

float UTrampleSubsystem::GetUploadIntervalSeconds(float FullClearSeconds)
{
    // Ровно одна ступень RGBA8: чаще -- выгружать ту же картинку, реже --
    // пропускать видимые ступени распада.
    return FMath::Max(FullClearSeconds, 0.0f) / 255.0f;
}

float UTrampleSubsystem::GetValueAt(const FVector2D& WorldXY) const
{
    return Field.GetValueAt(WorldXY, GetNowSeconds(),
        [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); });
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

URuntimeVirtualTextureComponent* UTrampleSubsystem::FindVolume() const
{
    if (URuntimeVirtualTextureComponent* Cached = CachedVolume.Get())
    {
        return Cached;
    }
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const URuntimeVirtualTexture* Texture = Settings ? Settings->TrampleVirtualTexture.LoadSynchronous() : nullptr;
    UWorld* World = GetWorld();
    if (!Texture || !World)
    {
        return nullptr;
    }
    for (TActorIterator<ARuntimeVirtualTextureVolume> It(World); It; ++It)
    {
        URuntimeVirtualTextureComponent* Component = It->VirtualTextureComponent;
        if (Component && Component->GetVirtualTexture() == Texture)
        {
            CachedVolume = Component;
            return Component;
        }
    }
    return nullptr;
}

float UTrampleSubsystem::GetDisplayRadiusCm() const
{
    // Радиус симуляции мира: тропа видна там же, где стоят ресурсы и живут
    // сущности. -1 у настройки -- стриминг выключен, показываем всё. Плюс
    // полдиагонали чанка: радиус меряется до центра чанка, а виден он краем.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Meters = Settings ? Settings->ActiveSimulationRadiusMeters : -1.0f;
    if (Meters < 0.0f)
    {
        return TNumericLimits<float>::Max();
    }
    return Meters * 100.0f + FTrampleField::ChunkSizeCm * UE_INV_SQRT_2;
}

void UTrampleSubsystem::RefreshDisplays(const FVector2D& ViewerXY)
{
    const float Now = GetNowSeconds();
    auto ClearSeconds = [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); };

    // 1. Чанки вдали от зрителя не выгружаются, но распадаться обязаны, иначе
    // память росла бы до возвращения игрока. Раз в ступень RGBA8 при базовом
    // периоде -- тот же такт, что у выгрузки.
    const float FullAdvanceInterval = GetUploadIntervalSeconds(ClearSeconds(FIntPoint::ZeroValue));
    if (LastFullAdvanceSeconds < 0.0f || Now < LastFullAdvanceSeconds || Now - LastFullAdvanceSeconds >= FullAdvanceInterval)
    {
        for (const FIntPoint& Removed : Field.AdvanceAll(Now, ClearSeconds))
        {
            DestroyDisplay(Removed);
        }
        LastFullAdvanceSeconds = Now;
    }

    // 2. Показ заводится только рядом с игроком.
    const double RadiusCm = GetDisplayRadiusCm();
    for (const TPair<FIntPoint, FTrampleField::FChunk>& Pair : Field.GetChunks())
    {
        if (FVector2D::Distance(FTrampleField::GetChunkCenter(Pair.Key), ViewerXY) <= RadiusCm)
        {
            EnsureDisplay(Pair.Key);
        }
    }

    TArray<FIntPoint> ToDestroy;
    for (const TPair<FIntPoint, FTrampleChunkDisplay>& Pair : Displays)
    {
        if (!Field.FindChunk(Pair.Key) || FVector2D::Distance(FTrampleField::GetChunkCenter(Pair.Key), ViewerXY) > RadiusCm)
        {
            ToDestroy.Add(Pair.Key);
        }
    }
    for (const FIntPoint& Coord : ToDestroy)
    {
        DestroyDisplay(Coord);
    }

    // 3. Выгрузка: сразу после штриха, иначе -- раз в ступень распада.
    FBox DirtyBounds(ForceInit);
    TArray<FIntPoint> Emptied;
    for (TPair<FIntPoint, FTrampleChunkDisplay>& Pair : Displays)
    {
        const FTrampleField::FChunk* Chunk = Field.FindChunk(Pair.Key);
        const float FullClear = ClearSeconds(Pair.Key);
        const bool bDue = Pair.Value.LastUploadSeconds < 0.0f
            || Now < Pair.Value.LastUploadSeconds
            || Now - Pair.Value.LastUploadSeconds >= GetUploadIntervalSeconds(FullClear);
        if (!Chunk || !(Chunk->bDirty || bDue))
        {
            continue;
        }

        if (Field.AdvanceChunk(Pair.Key, Now, FullClear))
        {
            Emptied.Add(Pair.Key);
            continue;
        }

        UploadChunk(Pair.Key, Pair.Value);
        Pair.Value.LastUploadSeconds = Now;
        Field.ClearDirty(Pair.Key);

        const FVector2D Origin = FTrampleField::GetChunkOrigin(Pair.Key);
        DirtyBounds += FVector(Origin, 0.0);
        DirtyBounds += FVector(Origin + FVector2D(FTrampleField::ChunkSizeCm), 0.0);
    }
    for (const FIntPoint& Coord : Emptied)
    {
        Field.RemoveChunk(Coord);
        if (FTrampleChunkDisplay* Display = Displays.Find(Coord))
        {
            // Последняя выгрузка -- чёрная, иначе страницы RVT держали бы
            // прошлую картинку уже удалённого чанка.
            UploadChunk(Coord, *Display);
            const FVector2D Origin = FTrampleField::GetChunkOrigin(Coord);
            DirtyBounds += FVector(Origin, 0.0);
            DirtyBounds += FVector(Origin + FVector2D(FTrampleField::ChunkSizeCm), 0.0);
        }
        DestroyDisplay(Coord);
    }

    // 4. Один Invalidate на кадр, по объединённой рамке.
    if (DirtyBounds.IsValid)
    {
        if (URuntimeVirtualTextureComponent* Volume = FindVolume())
        {
            const FBox VolumeBox = Volume->Bounds.GetBox();
            DirtyBounds.Min.Z = VolumeBox.Min.Z;
            DirtyBounds.Max.Z = VolumeBox.Max.Z;
            Volume->Invalidate(FBoxSphereBounds(DirtyBounds));
        }
    }
}

void UTrampleSubsystem::EnsureDisplay(const FIntPoint& ChunkCoord)
{
    if (Displays.Contains(ChunkCoord))
    {
        return;
    }

    // Без объёма RVT рисовать некуда: поле копится, показа нет. Так
    // L_TestDev, где объёма пока нет, не плодит бесполезных плоскостей.
    URuntimeVirtualTextureComponent* Volume = FindVolume();
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    URuntimeVirtualTexture* VirtualTexture = Settings ? Settings->TrampleVirtualTexture.LoadSynchronous() : nullptr;
    UMaterialInterface* WriterMaterial = Settings ? Settings->TrampleWriterMaterial.LoadSynchronous() : nullptr;
    UStaticMesh* PlaneMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Plane.Plane"));
    UWorld* World = GetWorld();
    if (!Volume || !VirtualTexture || !WriterMaterial || !PlaneMesh || !World)
    {
        if (!bWarnedMissingAssets && (!VirtualTexture || !WriterMaterial || !PlaneMesh))
        {
            UE_LOG(LogHerbalistWorld, Warning,
                TEXT("[Trample] Показ выключен: не назначены TrampleVirtualTexture/TrampleWriterMaterial в Herbalist Settings или нет /Engine/BasicShapes/Plane"));
            bWarnedMissingAssets = true;
        }
        return;
    }

    FTrampleChunkDisplay Display;

    // Те же флаги, что у RT_WorldStateMap: RGBA8 с линейной гаммой читается
    // сэмплером Linear Color -- ровно такой стоит у CurrentRT в M_RVTWriter.
    Display.Texture = NewObject<UTextureRenderTarget2D>(this);
    Display.Texture->RenderTargetFormat = RTF_RGBA8;
    Display.Texture->bForceLinearGamma = true;
    Display.Texture->Filter = TF_Bilinear;
    Display.Texture->AddressX = TA_Clamp;
    Display.Texture->AddressY = TA_Clamp;
    Display.Texture->ClearColor = FLinearColor::Black;
    Display.Texture->InitAutoFormat(FTrampleField::ChunkTexels, FTrampleField::ChunkTexels);
    Display.Texture->UpdateResourceImmediate(true);

    // Плоскость не поворачивается: её UV совпадают с мировыми X/Y только
    // без поворота. Z -- середина объёма, иначе RVT её не захватит.
    const FVector2D Center = FTrampleField::GetChunkCenter(ChunkCoord);
    const FVector Location(Center, Volume->Bounds.Origin.Z);
    FActorSpawnParameters SpawnParams;
    SpawnParams.ObjectFlags |= RF_Transient;
    SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    AStaticMeshActor* Plane = World->SpawnActor<AStaticMeshActor>(Location, FRotator::ZeroRotator, SpawnParams);
    if (!Plane)
    {
        return;
    }

    UStaticMeshComponent* Mesh = Plane->GetStaticMeshComponent();
    Mesh->SetMobility(EComponentMobility::Movable);
    Mesh->SetStaticMesh(PlaneMesh);
    Mesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    Mesh->SetCastShadow(false);
    Mesh->VirtualTextureRenderPassType = ERuntimeVirtualTextureMainPassType::Never;
    Mesh->RuntimeVirtualTextures.Add(VirtualTexture);

    // Меш -- 100x100 см.
    Plane->SetActorScale3D(FVector(FTrampleField::ChunkSizeCm / 100.0f, FTrampleField::ChunkSizeCm / 100.0f, 1.0f));

    Display.Material = UMaterialInstanceDynamic::Create(WriterMaterial, this);
    Display.Material->SetTextureParameterValue(Settings->TrampleWriterTextureParameter, Display.Texture);
    Mesh->SetMaterial(0, Display.Material);
    Mesh->MarkRenderStateDirty();

    Display.WriterPlane = Plane;
    Displays.Add(ChunkCoord, Display);
}

void UTrampleSubsystem::DestroyDisplay(const FIntPoint& ChunkCoord)
{
    FTrampleChunkDisplay Display;
    if (!Displays.RemoveAndCopyValue(ChunkCoord, Display))
    {
        return;
    }
    if (Display.WriterPlane)
    {
        Display.WriterPlane->Destroy();
    }
    if (Display.Texture)
    {
        Display.Texture->ReleaseResource();
    }
}

void UTrampleSubsystem::UploadChunk(const FIntPoint& ChunkCoord, FTrampleChunkDisplay& Display)
{
    if (!Display.Texture)
    {
        return;
    }
    FTextureRenderTargetResource* Resource = Display.Texture->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    TArray<FColor> Pixels;
    Field.BuildChunkPixels(ChunkCoord, Pixels);
    const int32 Size = FTrampleField::ChunkTexels;

    // Тот же путь, что у RT_WorldStateMap (GridWorldManagerWorldStateMap.cpp).
    ENQUEUE_RENDER_COMMAND(HerbalistTrampleUpload)(
        [Resource, Pixels = MoveTemp(Pixels), Size](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* Texture = Resource->GetRenderTargetTexture();
            if (!Texture) return;

            const FUpdateTextureRegion2D Region(0, 0, 0, 0, Size, Size);
            RHICmdList.UpdateTexture2D(Texture, 0, Region, Size * sizeof(FColor),
                reinterpret_cast<const uint8*>(Pixels.GetData()));
        });
}

TArray<FSavedTrampleChunk> UTrampleSubsystem::CaptureSaveChunks()
{
    // Сохраняем уже распавшиеся значения: при загрузке отсчёт идёт заново
    // от часов загруженной сессии, время между сессиями игровым не является.
    const float Now = GetNowSeconds();
    for (const FIntPoint& Removed : Field.AdvanceAll(Now, [this](const FIntPoint& Coord) { return GetFullClearSeconds(Coord); }))
    {
        DestroyDisplay(Removed);
    }

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
    LastFullAdvanceSeconds = Now;
}

void UTrampleSubsystem::ResetTrample()
{
    TArray<FIntPoint> Coords;
    Displays.GetKeys(Coords);
    for (const FIntPoint& Coord : Coords)
    {
        DestroyDisplay(Coord);
    }
    Field.Reset();
    LastWalkerXY.Reset();
    LastFullAdvanceSeconds = -1.0f;
}

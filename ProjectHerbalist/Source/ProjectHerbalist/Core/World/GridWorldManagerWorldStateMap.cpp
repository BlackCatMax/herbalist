// Core/World/GridWorldManagerWorldStateMap.cpp
//
// Карта состояния мира в текстуру (2026-09-07, "план A" по итогам разбора
// референсного проекта CalystoWorld). Задача: дать материалам травы и
// ландшафта читать состояние симуляции по мировой позиции, чтобы порча
// была видна глазом.
//
// ПОЧЕМУ ТЕКСТУРА, А НЕ PER-INSTANCE CUSTOM DATA. Первым планом было
// прокинуть Distortion в Custom Float Data спавнера и читать его в
// материале узлом PerInstanceCustomData. Разбор Calysto этот план
// отменил по двум причинам сразу:
//   1. Значение per-instance выставляется В МОМЕНТ СПАВНА и дальше не
//      меняется. Трава получила бы порчу один раз и застыла с ней
//      навсегда -- выглядело бы готовой работой (цвет разный по клеткам,
//      граница биомов видна), не будучи ею.
//   2. Все четыре StaticMeshSpawner в PCG_Grass стоят с bExecuteOnGPU,
//      а данные инстансов на этом пути живут в буферах GPU -- менять их
//      с игрового потока нечем.
// В самом Calysto MaterialExpressionPerInstanceCustomData не встречается
// НИ РАЗУ (проверено поиском по всем ассетам проекта): непрерывный отклик
// там делается пространственными источниками -- слоями ландшафта и RVT,
// которые материал сэмплирует по мировой позиции. Здесь то же самое, но
// источник -- живая сетка симуляции.
//
// ОДИН ИСТОЧНИК, ДВА ПОТРЕБИТЕЛЯ. Та же текстура обслуживает и траву, и
// ландшафт, как в Calysto одни и те же grass maps читают и HLSL-генератор
// растительности, и материал земли. Заводить отдельный канал под каждого
// потребителя незачем.
//
// ПОБОЧНО ЛЕЧИТСЯ ШОВ НА 96 МЕТРАХ. У PCG-узла Get Herbalist Grid
// bOnlyActiveCells=true, а активный радиус -- 100 м (DefaultGame.ini), то
// есть данные о мире он отдаёт только вокруг игрока. Любой отклик,
// построенный на его выводе, дал бы видимую границу по краю радиуса.
// Текстура покрывает окно, которое шире загруженного мира, и такой границы
// не имеет.
//
// ОКНО (2026-09-13, разметка мира, этап 7). Текстура размером со всю сетку
// на большом мире -- мегабайты раз в секунду ради земли, которой рядом нет.
// С разметкой в текстуру идёт окно WorldStateWindowCells клеток: 2 x
// (дальность загрузки + страница), до степени двойки -- на L_TestDev 128 x 128
// (1152 м) против сетки 224 x 224. Окно -- движущийся угол, а не адресация по
// кругу, как у троп (FTrampleWindow): материалы (M_landscape,
// M_Foliage_Master) уже строят UV как (позиция - начало) / размер, и при
// переезде окна достаточно сдвинуть начало -- материалы не меняются.

#include "Core/World/GridWorldManager.h"

#include "Core/Config/HerbalistSettings.h"
#include "Core/Shrine/ShrineTypes.h"
#include "HerbalistLogChannels.h"

#include "Engine/TextureRenderTarget2D.h"
#include "RenderingThread.h"
#include "RHICommandList.h"
#include "TextureResource.h"
#include "Materials/MaterialParameterCollection.h"
#include "Kismet/KismetMaterialLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"

namespace
{
    // 8 бит на канал: шаг квантования 1/255 ~= 0.0039. Обоснование, почему
    // этого достаточно, -- у WorldStateMapUpdateIntervalSeconds в заголовке.
    uint8 Quantize01(float Value)
    {
        const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
        return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Clamped * 255.0f), 0, 255));
    }

    // Двигает Current к Target не быстрее MaxDelta. Именно ограничение
    // скорости, а не экспоненциальное приближение -- довод у
    // WorldStateMapVisualRatePerSecond в заголовке.
    float StepToward(float Current, float Target, float MaxDelta)
    {
        if (MaxDelta <= 0.0f)
        {
            return Target;
        }
        return Current + FMath::Clamp(Target - Current, -MaxDelta, MaxDelta);
    }
}

FVector4f AGridWorldManager::GetCellWorldStateAxes(const FGridCell& Cell) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 ShrineRadius = GetCellRadius(Settings ? Settings->ShrineInfluenceRadiusMeters : 30.0f);

    // Каналы выбраны не произвольно: это ровно те четыре оси, которые шапка
    // PCGHerbalistGridData.h называет практическим применением обратной
    // связи "симуляция -> вид мира" ("почерневшая от Морока трава",
    // "вытоптанная поляна редеет", "вокруг ухоженного капища гуще").
    // Четыре канала стоят столько же, сколько один.
    return FVector4f(
        Cell.State.Meta.Distortion,
        Cell.State.Meta.Corruption,
        Cell.HarvestStress,
        HerbalistCore::Shrine::GetInfluenceAt(
            FIntPoint(Cell.X, Cell.Y), GetShrines(), ShrineRadius));
}

void AGridWorldManager::GetWorldStateWindow(FIntPoint& OutMinCell, FIntPoint& OutSize) const
{
    const FIntPoint GridMin = GetGridMinCell();
    const int32 WindowCells = ResolvedLayout.bValid ? ResolvedLayout.WorldStateWindowCells : 0;

    // Без разметки или когда окно не меньше сетки -- окно во всю сетку: один
    // тексель на клетку всей сетки, как до окна.
    if (WindowCells <= 0 || (WindowCells >= GridSizeX && WindowCells >= GridSizeY))
    {
        OutMinCell = GridMin;
        OutSize = FIntPoint(GridSizeX, GridSizeY);
        return;
    }

    OutSize = FIntPoint(FMath::Min(WindowCells, GridSizeX), FMath::Min(WindowCells, GridSizeY));
    const FIntPoint Desired = bWorldStateWindowPlaced
        ? WorldStateWindowMin
        : ComputeWorldStateWindowMin(GridMin + FIntPoint(GridSizeX / 2, GridSizeY / 2), OutSize);

    // Зажатие и у поставленного окна (ревью этапа 7): сетка могла смениться
    // после расстановки.
    OutMinCell.X = FMath::Clamp(Desired.X, GridMin.X, GridMin.X + GridSizeX - OutSize.X);
    OutMinCell.Y = FMath::Clamp(Desired.Y, GridMin.Y, GridMin.Y + GridSizeY - OutSize.Y);
}

int32 AGridWorldManager::GetWorldStateWindowTile() const
{
    // Восьмая часть окна, как у окна троп. От окна разметки, а не от размера,
    // зажатого сеткой (ревью этапа 7): у сетки, узкой по одной оси, тайл иначе
    // сжался бы до пары клеток, и окно переезжало бы на каждом шаге.
    const int32 WindowCells = ResolvedLayout.bValid ? ResolvedLayout.WorldStateWindowCells : 0;
    return FMath::Max(1, WindowCells / 8);
}

FIntPoint AGridWorldManager::ComputeWorldStateWindowMin(const FIntPoint& ViewerCell, const FIntPoint& Size) const
{
    // Угол -- к ближайшему кратному тайла, а не вниз (ревью этапа 7): после
    // перестановки зритель не дальше полутайла от центра окна по каждой оси.
    // Целочисленно, как номера чанков.
    const int32 Tile = GetWorldStateWindowTile();
    auto SnapAxis = [Tile](int32 Desired)
    {
        return HerbalistCore::FloorDivCoord(Desired + Tile / 2, Tile) * Tile;
    };

    const FIntPoint GridMin = GetGridMinCell();
    FIntPoint Min(SnapAxis(ViewerCell.X - Size.X / 2), SnapAxis(ViewerCell.Y - Size.Y / 2));
    Min.X = FMath::Clamp(Min.X, GridMin.X, GridMin.X + GridSizeX - Size.X);
    Min.Y = FMath::Clamp(Min.Y, GridMin.Y, GridMin.Y + GridSizeY - Size.Y);
    return Min;
}

bool AGridWorldManager::GetWorldStateViewerCell(FIntPoint& OutCell) const
{
    // Точка обзора локального игрока 0 (ревью этапа 7): текстура и MPC одни на
    // мир, и окно следует за тем, кто смотрит, а не за первым источником
    // стриминга (транспорт, престриминг телепорта). Разделённый экран одним
    // окном не обслужить -- окно у игрока 0.
    const UWorld* World = GetWorld();
    const APlayerController* PlayerController = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;
    if (PlayerController && CellSize > 0.0f)
    {
        FVector ViewLocation;
        FRotator ViewRotation;
        PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);
        const FVector Local = ViewLocation - GetCellWorldPositionFlat(0, 0);
        OutCell = FIntPoint(FMath::FloorToInt32(Local.X / CellSize), FMath::FloorToInt32(Local.Y / CellSize));
        return true;
    }

    // Без игрока (автотесты, редактор) -- центр первого чанка активности.
    if (ActiveChunkCenters.Num() > 0)
    {
        const int32 ChunkSize = GetChunkSizeInCells();
        const FIntPoint& Chunk = ActiveChunkCenters[0];
        OutCell = FIntPoint(Chunk.X * ChunkSize + ChunkSize / 2, Chunk.Y * ChunkSize + ChunkSize / 2);
        return true;
    }
    return false;
}

bool AGridWorldManager::IsWorldStateWindowStale() const
{
    if (!bWorldStateWindowPlaced)
    {
        return false;
    }

    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    FIntPoint Viewer;
    if ((Size.X == GridSizeX && Size.Y == GridSizeY) || !GetWorldStateViewerCell(Viewer))
    {
        return false;
    }

    // Гистерезис (ревью этапа 7): перестановка -- только когда зритель ушёл от
    // центра дальше целого тайла. Новый угол ставит его не дальше полутайла от
    // центра, и обратно окно переедет лишь после ещё полутайла пути: у границы
    // тайла окно не прыгает туда-обратно. Запас до края окна -- полокна минус
    // тайл: на L_TestDev 64 - 16 = 48 клеток (432 м) против 378 м загруженного
    // мира (дальность 252 м + страница 126 м). Окно, прижатое к краю сетки,
    // устаревшим не считается, пока новый угол тот же.
    const int32 Tile = GetWorldStateWindowTile();
    const FIntPoint Center = Min + FIntPoint(Size.X / 2, Size.Y / 2);
    const bool bFar = FMath::Abs(Viewer.X - Center.X) > Tile || FMath::Abs(Viewer.Y - Center.Y) > Tile;
    return bFar && ComputeWorldStateWindowMin(Viewer, Size) != Min;
}

bool AGridWorldManager::UpdateWorldStateWindow()
{
    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    if (Size.X == GridSizeX && Size.Y == GridSizeY)
    {
        bWorldStateWindowPlaced = false;
        return false;
    }

    // Без зрителя окно стоит, где стояло (до первой расстановки -- по центру
    // сетки).
    FIntPoint NewMin = Min;
    FIntPoint Viewer;
    if ((!bWorldStateWindowPlaced || IsWorldStateWindowStale()) && GetWorldStateViewerCell(Viewer))
    {
        NewMin = ComputeWorldStateWindowMin(Viewer, Size);
    }

    const bool bMoved = !bWorldStateWindowPlaced || NewMin != WorldStateWindowMin;
    WorldStateWindowMin = NewMin;
    bWorldStateWindowPlaced = true;
    return bMoved;
}

void AGridWorldManager::SnapWorldStateMapDisplayToWorld()
{
    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    if (Size.X <= 0 || Size.Y <= 0 || Cells.Num() == 0)
    {
        DisplayedWorldState.Reset();
        return;
    }

    DisplayedWorldState.SetNumZeroed(Size.X * Size.Y);
    DisplayedWorldStateMin = Min;
    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        for (int32 X = 0; X < Size.X; ++X)
        {
            if (const FGridCell* Cell = GetCellConst(Min.X + X, Min.Y + Y))
            {
                DisplayedWorldState[Y * Size.X + X] = GetCellWorldStateAxes(*Cell);
            }
        }
    }
}

void AGridWorldManager::ShiftWorldStateMapDisplay(const FIntPoint& NewMin, const FIntPoint& Size)
{
    // Переезд окна (ревью этапа 7): при тайле в восьмую часть окна не меньше
    // 7/8 клеток остаются в окне, и их сглаживание не сбрасывается -- иначе на
    // каждом переезде они скачком приравнивались бы к миру посреди перехода.
    TArray<FVector4f> Shifted;
    Shifted.SetNumZeroed(Size.X * Size.Y);
    const FIntPoint OldMin = DisplayedWorldStateMin;
    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        for (int32 X = 0; X < Size.X; ++X)
        {
            const int32 OldX = NewMin.X + X - OldMin.X;
            const int32 OldY = NewMin.Y + Y - OldMin.Y;
            if (OldX >= 0 && OldX < Size.X && OldY >= 0 && OldY < Size.Y)
            {
                Shifted[Y * Size.X + X] = DisplayedWorldState[OldY * Size.X + OldX];
            }
            else if (const FGridCell* Cell = GetCellConst(NewMin.X + X, NewMin.Y + Y))
            {
                Shifted[Y * Size.X + X] = GetCellWorldStateAxes(*Cell);
            }
        }
    }
    DisplayedWorldState = MoveTemp(Shifted);
    DisplayedWorldStateMin = NewMin;
}

void AGridWorldManager::AdvanceWorldStateMapDisplay(float DeltaSeconds)
{
    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    if (Size.X <= 0 || Size.Y <= 0 || Cells.Num() == 0)
    {
        return;
    }

    // Первый вызов или сетка сменила размер -- приравниваем, а не догоняем:
    // иначе при загрузке уровня мир выцветал бы из нулей на глазах у игрока,
    // изображая изменение, которого не было. Окно переехало -- сдвигаем показ.
    if (DisplayedWorldState.Num() != Size.X * Size.Y)
    {
        SnapWorldStateMapDisplayToWorld();
        return;
    }
    if (DisplayedWorldStateMin != Min)
    {
        ShiftWorldStateMapDisplay(Min, Size);
    }

    // Время не прошло (внеочередная выгрузка из Tick) -- шагать нечем. Нулевой
    // шаг у StepToward значит «мгновенно» (нулевая скорость), и без этого
    // выхода внеочередная выгрузка приравнивала бы всё окно к миру.
    if (DeltaSeconds <= 0.0f)
    {
        return;
    }

    const float MaxDelta = WorldStateMapVisualRatePerSecond * DeltaSeconds;

    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        for (int32 X = 0; X < Size.X; ++X)
        {
            const FGridCell* Cell = GetCellConst(Min.X + X, Min.Y + Y);
            if (!Cell)
            {
                continue;
            }

            const FVector4f Target = GetCellWorldStateAxes(*Cell);
            FVector4f& Shown = DisplayedWorldState[Y * Size.X + X];

            Shown.X = StepToward(Shown.X, Target.X, MaxDelta);
            Shown.Y = StepToward(Shown.Y, Target.Y, MaxDelta);
            Shown.Z = StepToward(Shown.Z, Target.Z, MaxDelta);
            Shown.W = StepToward(Shown.W, Target.W, MaxDelta);
        }
    }
}

TArray<FColor> AGridWorldManager::BuildWorldStateMapPixels() const
{
    TArray<FColor> Pixels;

    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    if (Size.X <= 0 || Size.Y <= 0 || Cells.Num() == 0)
    {
        return Pixels;
    }

    // Раскладка тексель-в-клетку: индекс пикселя -- (Y - угол) * ширина окна
    // + (X - угол), та же, что у GetWorldStateMapUV. Любое расхождение здесь
    // означало бы, что материал красит не ту клетку, причём тихо и
    // правдоподобно.
    Pixels.SetNumZeroed(Size.X * Size.Y);

    // Читаем ПОКАЗЫВАЕМОЕ состояние, а не истинное: разница между ними и
    // есть сглаживание рывков от дискретных событий, см.
    // AdvanceWorldStateMapDisplay. Если сглаживание ещё не
    // инициализировано для этого окна, показываем истинное -- правильный кадр
    // без сглаживания лучше чёрного.
    const bool bHasDisplayState = DisplayedWorldState.Num() == Size.X * Size.Y && DisplayedWorldStateMin == Min;

    for (int32 Y = 0; Y < Size.Y; ++Y)
    {
        for (int32 X = 0; X < Size.X; ++X)
        {
            const FGridCell* Cell = GetCellConst(Min.X + X, Min.Y + Y);
            if (!Cell)
            {
                continue;
            }

            const int32 Index = Y * Size.X + X;
            const FVector4f Axes = bHasDisplayState ? DisplayedWorldState[Index] : GetCellWorldStateAxes(*Cell);

            Pixels[Index] = FColor(
                Quantize01(Axes.X),   // R -- непрерывная порча
                Quantize01(Axes.Y),   // G -- ось бистабильности
                Quantize01(Axes.Z),   // B -- вытоптанность
                Quantize01(Axes.W));  // A -- влияние капищ
        }
    }

    return Pixels;
}

bool AGridWorldManager::GetWorldStateMapUV(const FVector& WorldPosition, FVector2D& OutUV) const
{
    OutUV = FVector2D::ZeroVector;

    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    if (Size.X <= 0 || Size.Y <= 0 || CellSize <= 0.0f)
    {
        return false;
    }

    // Та же система отсчёта, что у WorldPositionToCell: от угла клетки, деление
    // на CellSize, пол. GetCellWorldPositionFlat отдаёт УГОЛ клетки, а не центр
    // (клетка X занимает [X*CellSize, (X+1)*CellSize)) -- если бы карта считала
    // от центра, она разъехалась бы с симуляцией на полклетки. Это
    // проверяется тестом WorldStateMapTest.UVAgreesWithWorldPositionToCell,
    // а не оставлено на веру.
    const FVector Local = WorldPosition - GetCellWorldPositionFlat(Min.X, Min.Y);
    OutUV = FVector2D(
        Local.X / (Size.X * CellSize),
        Local.Y / (Size.Y * CellSize));

    return OutUV.X >= 0.0f && OutUV.X < 1.0f && OutUV.Y >= 0.0f && OutUV.Y < 1.0f;
}

void AGridWorldManager::GetWorldStateMapFrame(FVector& OutOrigin, FVector2D& OutWorldSize) const
{
    FIntPoint Min;
    FIntPoint Size;
    GetWorldStateWindow(Min, Size);
    OutOrigin = GetCellWorldPositionFlat(Min.X, Min.Y);
    OutWorldSize = FVector2D(Size.X * CellSize, Size.Y * CellSize);
}

void AGridWorldManager::UpdateWorldStateMap()
{
    // Таймер: шаг сглаживания равен периоду -- он и есть время, прошедшее с
    // прошлой выгрузки.
    UploadWorldStateMap(WorldStateMapUpdateIntervalSeconds);
}

void AGridWorldManager::UploadWorldStateMap(float DisplayStepSeconds)
{
    // Окно -- до рамки и пикселей: всё ниже описывает одно и то же окно.
    UpdateWorldStateWindow();

    FVector Origin;
    FVector2D WorldSize;
    GetWorldStateMapFrame(Origin, WorldSize);
    auto WriteFrame = [this, &Origin, &WorldSize]()
    {
        UWorld* CurrentWorld = GetWorld();
        UMaterialParameterCollection* Frame = CurrentWorld ? WorldStateFrameCollection.LoadSynchronous() : nullptr;
        if (!Frame)
        {
            return;
        }
        UKismetMaterialLibrary::SetVectorParameterValue(CurrentWorld, Frame,
            TEXT("WorldStateMapOrigin"),
            FLinearColor(Origin.X, Origin.Y, Origin.Z, 0.0f));
        UKismetMaterialLibrary::SetVectorParameterValue(CurrentWorld, Frame,
            TEXT("WorldStateMapSize"),
            FLinearColor(WorldSize.X, WorldSize.Y, 0.0f, 0.0f));
    };

    UTextureRenderTarget2D* Target = WorldStateMap.LoadSynchronous();
    if (!Target)
    {
        // Карта не назначена -- механизм выключен, это не ошибка. Рамка
        // материалу нужна и так, и стоит ровно ничего (две записи в MPC).
        WriteFrame();
        return;
    }

    // Сначала двигаем картинку к миру (переехавшее окно сдвигает показ),
    // потом снимаем её.
    AdvanceWorldStateMapDisplay(DisplayStepSeconds);

    TArray<FColor> Pixels = BuildWorldStateMapPixels();
    if (Pixels.Num() == 0)
    {
        return;
    }

    // Разрешение НЕ настраивается: один тексель на клетку окна. Мельче --
    // выдумывать детализацию, которой в симуляции нет (у клетки нет
    // внутренней структуры); крупнее -- терять существующую. Поэтому цель
    // подгоняется под окно, а не наоборот, и рассинхрон настроек невозможен
    // в принципе.
    FIntPoint WindowMin;
    FIntPoint WindowSize;
    GetWorldStateWindow(WindowMin, WindowSize);
    if (Target->SizeX != WindowSize.X || Target->SizeY != WindowSize.Y)
    {
        Target->ResizeTarget(WindowSize.X, WindowSize.Y);
    }

    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    const int32 Width = WindowSize.X;
    const int32 Height = WindowSize.Y;

    ENQUEUE_RENDER_COMMAND(HerbalistWorldStateMapUpload)(
        [Resource, Pixels = MoveTemp(Pixels), Width, Height](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* Texture = Resource->GetRenderTargetTexture();
            if (!Texture)
            {
                return;
            }

            const FUpdateTextureRegion2D Region(0, 0, 0, 0, Width, Height);
            RHICmdList.UpdateTexture2D(
                Texture, 0, Region,
                Width * sizeof(FColor),
                reinterpret_cast<const uint8*>(Pixels.GetData()));
        });

    // Рамка -- после постановки выгрузки в очередь (ревью этапа 7): обе
    // применяются к кадру этого тика (MPC -- в SendAllEndOfFrameUpdates), а
    // при выходе выше без выгрузки старая рамка остаётся при старых текселях.
    WriteFrame();
}

bool AGridWorldManager::IsWorldStateMapUpdateScheduled() const
{
    return GetWorldTimerManager().IsTimerActive(WorldStateMapTimerHandle);
}

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
// Текстура покрывает сетку ЦЕЛИКОМ и такой границы не имеет.

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

namespace
{
    // 8 бит на канал: шаг квантования 1/255 ~= 0.0039. Обоснование, почему
    // этого достаточно, -- у WorldStateMapUpdateIntervalSeconds в заголовке.
    uint8 Quantize01(float Value)
    {
        const float Clamped = FMath::Clamp(Value, 0.0f, 1.0f);
        return static_cast<uint8>(FMath::Clamp(FMath::RoundToInt(Clamped * 255.0f), 0, 255));
    }
}

TArray<FColor> AGridWorldManager::BuildWorldStateMapPixels() const
{
    TArray<FColor> Pixels;

    if (GridSizeX <= 0 || GridSizeY <= 0 || Cells.Num() == 0)
    {
        return Pixels;
    }

    // Раскладка тексель-в-клетку: индекс пикселя == GetCellIndex(X, Y) ==
    // Y * GridSizeX + X. Не "похожая" формула, а буквально та же -- любое
    // расхождение здесь означало бы, что материал красит не ту клетку,
    // причём тихо и правдоподобно.
    Pixels.SetNumZeroed(GridSizeX * GridSizeY);

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 ShrineRadius = Settings ? Settings->ShrineInfluenceRadius : 3;
    const TArray<FShrine>& ShrineList = GetShrines();

    ForEachCell([&](const FGridCell& Cell)
    {
        const int32 Index = GetCellIndex(Cell.X, Cell.Y);
        if (!Pixels.IsValidIndex(Index)) return;

        // Каналы выбраны не произвольно: это ровно те четыре оси, которые
        // шапка PCGHerbalistGridData.h называет практическим применением
        // обратной связи "симуляция -> вид мира" ("почерневшая от Морока
        // трава", "вытоптанная поляна редеет", "вокруг ухоженного капища
        // гуще"). Четыре канала стоят столько же, сколько один, поэтому
        // класть сюда меньше -- значит гарантировать вторую такую же
        // текстуру через месяц.
        const float Restoration = HerbalistCore::Shrine::GetInfluenceAt(
            FIntPoint(Cell.X, Cell.Y), ShrineList, ShrineRadius);

        Pixels[Index] = FColor(
            Quantize01(Cell.State.Meta.Distortion),   // R -- непрерывная порча
            Quantize01(Cell.State.Meta.Corruption),   // G -- ось бистабильности
            Quantize01(Cell.HarvestStress),           // B -- вытоптанность
            Quantize01(Restoration));                 // A -- влияние капищ
    });

    return Pixels;
}

bool AGridWorldManager::GetWorldStateMapUV(const FVector& WorldPosition, FVector2D& OutUV) const
{
    OutUV = FVector2D::ZeroVector;

    if (GridSizeX <= 0 || GridSizeY <= 0 || CellSize <= 0.0f)
    {
        return false;
    }

    // Та же система отсчёта, что у WorldPositionToCell: локальные
    // координаты от GetActorLocation(), деление на CellSize, пол.
    // GetCellWorldPositionFlat отдаёт УГОЛ клетки, а не центр (клетка X
    // занимает [X*CellSize, (X+1)*CellSize)) -- если бы карта считала
    // от центра, она разъехалась бы с симуляцией на полклетки. Это
    // проверяется тестом WorldStateMapTest.UVAgreesWithWorldPositionToCell,
    // а не оставлено на веру.
    const FVector Local = WorldPosition - GetActorLocation();
    OutUV = FVector2D(
        Local.X / (GridSizeX * CellSize),
        Local.Y / (GridSizeY * CellSize));

    return OutUV.X >= 0.0f && OutUV.X < 1.0f && OutUV.Y >= 0.0f && OutUV.Y < 1.0f;
}

void AGridWorldManager::GetWorldStateMapFrame(FVector& OutOrigin, FVector2D& OutWorldSize) const
{
    OutOrigin = GetActorLocation();
    OutWorldSize = FVector2D(GridSizeX * CellSize, GridSizeY * CellSize);
}

void AGridWorldManager::UpdateWorldStateMap()
{
    // Рамка карты -- отдельно от самой выгрузки и ДО проверки цели: она
    // нужна материалу, даже если render target ещё не назначен, и стоит
    // ровно ничего (две записи в MPC раз в секунду).
    if (UWorld* CurrentWorld = GetWorld())
    {
        if (UMaterialParameterCollection* Frame = WorldStateFrameCollection.LoadSynchronous())
        {
            FVector Origin;
            FVector2D WorldSize;
            GetWorldStateMapFrame(Origin, WorldSize);

            UKismetMaterialLibrary::SetVectorParameterValue(CurrentWorld, Frame,
                TEXT("WorldStateMapOrigin"),
                FLinearColor(Origin.X, Origin.Y, Origin.Z, 0.0f));
            UKismetMaterialLibrary::SetVectorParameterValue(CurrentWorld, Frame,
                TEXT("WorldStateMapSize"),
                FLinearColor(WorldSize.X, WorldSize.Y, 0.0f, 0.0f));
        }
    }

    UTextureRenderTarget2D* Target = WorldStateMap.LoadSynchronous();
    if (!Target)
    {
        return;   // Карта не назначена -- механизм просто выключен, это не ошибка.
    }

    TArray<FColor> Pixels = BuildWorldStateMapPixels();
    if (Pixels.Num() == 0)
    {
        return;
    }

    // Разрешение НЕ настраивается: один тексель на клетку. Мельче --
    // выдумывать детализацию, которой в симуляции нет (у клетки нет
    // внутренней структуры); крупнее -- терять существующую. Поэтому цель
    // подгоняется под сетку, а не наоборот, и рассинхрон настроек
    // невозможен в принципе.
    if (Target->SizeX != GridSizeX || Target->SizeY != GridSizeY)
    {
        Target->ResizeTarget(GridSizeX, GridSizeY);
    }

    FTextureRenderTargetResource* Resource = Target->GameThread_GetRenderTargetResource();
    if (!Resource)
    {
        return;
    }

    const int32 Width = GridSizeX;
    const int32 Height = GridSizeY;

    ENQUEUE_RENDER_COMMAND(HerbalistWorldStateMapUpload)(
        [Resource, Pixels = MoveTemp(Pixels), Width, Height](FRHICommandListImmediate& RHICmdList)
        {
            FRHITexture* Texture = Resource->GetRenderTargetTexture();
            if (!Texture) return;

            const FUpdateTextureRegion2D Region(0, 0, 0, 0, Width, Height);
            RHICmdList.UpdateTexture2D(
                Texture, 0, Region,
                Width * sizeof(FColor),
                reinterpret_cast<const uint8*>(Pixels.GetData()));
        });
}

bool AGridWorldManager::IsWorldStateMapUpdateScheduled() const
{
    return GetWorldTimerManager().IsTimerActive(WorldStateMapTimerHandle);
}

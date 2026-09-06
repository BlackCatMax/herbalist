// Core/World/GridWorldManagerPOI.cpp
//
// Точки интереса (DESIGN_Brewing_Situations_And_Lore.md §4, 2026-09-06,
// прямой запрос пользователя "проработаем POI — кроме курганов должны
// быть ещё точки, фольклорные места, постройки"). См. POITypes.h за общим
// доводом каркаса и GridWorldManager.h за доводом у полей/геттеров каждого
// вида точки.

#include "Core/World/GridWorldManager.h"
#include "Core/World/POIActors.h"
#include "Core/Config/HerbalistSettings.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

namespace
{
    // Один и тот же детерминированный линейный проход, что уже
    // SeedKurganSites -- первая небезводная клетка, начиная со случайного
    // (тем же WorldRNG) сдвига, ещё не занятая другим POI. Не retry-цикл со
    // случайными координатами (гарантированно завершается за один проход
    // по сетке даже на маленькой тестовой сетке).
    FIntPoint SeedSinglePOISite(TArray<FGridCell>& Cells, FRandomStream& WorldRNG, const TSet<FIntPoint>& Occupied)
    {
        const int32 TotalCells = Cells.Num();
        if (TotalCells == 0) return FIntPoint(-1, -1);

        const int32 StartIndex = WorldRNG.RandRange(0, TotalCells - 1);
        for (int32 Offset = 0; Offset < TotalCells; ++Offset)
        {
            const FGridCell& Cell = Cells[(StartIndex + Offset) % TotalCells];
            if (Cell.bIsWater) continue;

            const FIntPoint Coord(Cell.X, Cell.Y);
            if (Occupied.Contains(Coord)) continue;
            return Coord;
        }
        return FIntPoint(-1, -1);
    }
}

void AGridWorldManager::SeedPointsOfInterest()
{
    // Курганы сеются первыми и своим собственным, уже протестированным
    // кодом -- их поведение (ровно 2 клетки, свой проход) не меняется,
    // только вызов переносится под эту общую точку входа.
    SeedKurganSites();

    // Занятые клетки — курганы (свежесеянные строкой выше) и всё, что уже
    // расставили SeedTestLandmarks/SeedLegendaryAnchors чуть раньше в этой
    // же InitializeCells (найдено 2026-09-06 регрессией
    // Herbalist.Landmark.SeedTestLandmarksGivesEachDefinitionADistinctCell:
    // без этого Тотем/Светлояр/Горюч-камень/Соловей/Змей могли сесть на уже
    // занятую клетку "хозяина"-по-биому, а Змей вдобавок стал бы вторым,
    // недостижимым EntityLandmark на той же клетке).
    TSet<FIntPoint> Occupied;
    Occupied.Reserve(KurganSites.Num() + EntityLandmarks.Num() + LegendaryAnchors.Num() + 5);
    for (const auto& Pair : KurganSites)
    {
        Occupied.Add(Pair.Key);
    }
    for (const FEntityLandmark& Landmark : EntityLandmarks)
    {
        Occupied.Add(Landmark.Cell);
    }
    for (const auto& Pair : LegendaryAnchors)
    {
        Occupied.Add(Pair.Value);
    }

    TotemSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (TotemSite != FIntPoint(-1, -1))
    {
        Occupied.Add(TotemSite);
        // Актор-визуал (DESIGN_POI_Art_And_LevelDesign.md §1, 2026-09-06) --
        // спавнится самим менеджером в уже выбранной клетке, не размещается
        // вручную (в отличие от AShrineActor).
        if (UWorld* World = GetWorld())
        {
            if (APOI_Totem* TotemActor = World->SpawnActor<APOI_Totem>(APOI_Totem::StaticClass(),
                GetCellWorldPosition(TotemSite.X, TotemSite.Y), FRotator::ZeroRotator))
            {
                TotemActor->Init(this, TotemSite.X, TotemSite.Y);
            }
        }
    }

    SvetloyarSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (SvetloyarSite != FIntPoint(-1, -1))
    {
        Occupied.Add(SvetloyarSite);
        if (UWorld* World = GetWorld())
        {
            if (APOI_Svetloyar* SvetloyarActor = World->SpawnActor<APOI_Svetloyar>(APOI_Svetloyar::StaticClass(),
                GetCellWorldPosition(SvetloyarSite.X, SvetloyarSite.Y), FRotator::ZeroRotator))
            {
                SvetloyarActor->Init(this, SvetloyarSite.X, SvetloyarSite.Y);
            }
        }
    }

    GoryuchKamenSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (GoryuchKamenSite != FIntPoint(-1, -1))
    {
        Occupied.Add(GoryuchKamenSite);
        if (UWorld* World = GetWorld())
        {
            if (APOI_GoryuchKamen* StoneActor = World->SpawnActor<APOI_GoryuchKamen>(APOI_GoryuchKamen::StaticClass(),
                GetCellWorldPosition(GoryuchKamenSite.X, GoryuchKamenSite.Y), FRotator::ZeroRotator))
            {
                StoneActor->Init(this, GoryuchKamenSite.X, GoryuchKamenSite.Y);
            }
        }
    }

    SoloveySite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (SoloveySite != FIntPoint(-1, -1)) Occupied.Add(SoloveySite);

    // Калинов мост / Трёхглавый Змей (§4.4) -- в отличие от остальных выше,
    // сразу же становится Landmark (RegisterZmeyGorynych), не просто
    // координатой: взаимодействие идёт через уже существующий TalkTo/
    // ChooseDialogueBranch, а не отдельный запрос/Activate-метод.
    KalinovMostSite = SeedSinglePOISite(Cells, WorldRNG, Occupied);
    if (KalinovMostSite != FIntPoint(-1, -1))
    {
        Occupied.Add(KalinovMostSite);
        RegisterZmeyGorynych(KalinovMostSite);
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[POI] Seeded: Totem(%d,%d) Svetloyar(%d,%d) GoryuchKamen(%d,%d) Solovey(%d,%d) KalinovMost(%d,%d)"),
        TotemSite.X, TotemSite.Y, SvetloyarSite.X, SvetloyarSite.Y,
        GoryuchKamenSite.X, GoryuchKamenSite.Y, SoloveySite.X, SoloveySite.Y,
        KalinovMostSite.X, KalinovMostSite.Y);
}

FString AGridWorldManager::GetTotemRevealText() const
{
    if (TotemSite == FIntPoint(-1, -1)) return TEXT("Тотем не найден на этой карте.");

    const FGridCell* Cell = GetCellConst(TotemSite.X, TotemSite.Y);
    if (!Cell) return TEXT("Тотем не найден на этой карте.");

    // Нижний ярус (подземное божество / Навь, §4.2) — всегда читается,
    // прямое попадание в Distortion места.
    const float Distortion = Cell->State.Meta.Distortion;
    FString Result = FString::Printf(TEXT("Нижний ярус (подземное божество, Навь): Distortion места %.2f -- %s."),
        Distortion, Distortion >= 0.5f ? TEXT("лик искажён, черты плывут") : TEXT("лик читается ясно"));

    // Средний ярус — Молва (DESIGN_POI_Art_And_LevelDesign.md, юнит 2,
    // 2026-09-06). Закрывает честный пробел юнита 1/2 (не было структуры
    // для "состояния игрока") переиспользованием уже существующей общинной
    // переменной, не новой.
    if (IsTotemMiddleTierVisible())
    {
        Result += TEXT(" Средний ярус (люди, взявшиеся за руки) виден -- община держится за это место.");
    }

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float PurityThreshold = Settings ? Settings->TotemUpperTierPurityThreshold : 0.6f;
    if (Cell->State.Meta.Purity >= PurityThreshold)
    {
        Result += TEXT(" Верхний ярус (небо и боги) виден -- высокая Purity места открывает его.");
    }

    return Result;
}

bool AGridWorldManager::IsTotemMiddleTierVisible() const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Threshold = Settings ? Settings->TotemMiddleTierMolvaThreshold : 0.5f;
    return Molva >= Threshold;
}

bool AGridWorldManager::IsSvetloyarVisible() const
{
    if (SvetloyarSite == FIntPoint(-1, -1)) return false;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Threshold = Settings ? Settings->SvetloyarVisibilityClarityThreshold : 0.7f;
    return GlobalPerceptionClarity >= Threshold;
}

int32 AGridWorldManager::GetSvetloyarSoundTier() const
{
    if (!IsSvetloyarVisible()) return 0;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float BaseThreshold = Settings ? Settings->SvetloyarVisibilityClarityThreshold : 0.7f;
    const float Range = FMath::Max(1.0f - BaseThreshold, KINDA_SMALL_NUMBER);
    const float T = FMath::Clamp((GlobalPerceptionClarity - BaseThreshold) / Range, 0.0f, 1.0f);
    return (T >= 0.95f) ? 3 : (T >= 0.5f) ? 2 : 1;
}

bool AGridWorldManager::ActivateSolovey()
{
    if (SoloveySite == FIntPoint(-1, -1)) return false;

    // Усмирён плакун-травой (§4.4, 2026-09-06) -- проверяется ПЕРЕД
    // bSoloveyTriggered: игрок мог усмирить Соловья, ни разу не пройдя
    // мимо (CalmSolovey не требует контакта), первый же проход тогда
    // обязан быть безопасным, не только повторный.
    if (bSoloveyCalmed) return true;
    if (bSoloveyTriggered) return false;

    // Оберег ДО контакта (одолень-трава, §4.4) -- та же
    // IsWardConcealmentActive(Cell), что уже даёт "скрытие" от бестиария;
    // здесь просто другая угроза читает тот же флаг/радиус. Проход под
    // прикрытием засчитывается без Морока -- точка отмечается пройденной,
    // повторно не сработает.
    if (IsWardConcealmentActive(SoloveySite))
    {
        bSoloveyTriggered = true;
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Solovey] Passed at (%d,%d) under одолень-трава concealment -- no Morok"),
            SoloveySite.X, SoloveySite.Y);
        return true;
    }

    // Морок (DECISIONS_LOG.md §1: "локальный, временный акт искажения
    // восприятия... здесь и сейчас") -- разовая AoE-порча Purity/Stability
    // при контакте, ровно тот учебниковый случай, для которого термин
    // закреплён: локальная (радиус вокруг точки), временная (один тик, не
    // устойчивое состояние региона), не Навь.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->SoloveyCorruptionRadius : 3;
    const float Burst = Settings ? Settings->SoloveyCorruptionBurst : 0.3f;

    for (FGridCell& Cell : Cells)
    {
        const int32 Dist = FMath::Max(FMath::Abs(Cell.X - SoloveySite.X), FMath::Abs(Cell.Y - SoloveySite.Y));
        if (Dist > Radius) continue;

        Cell.State.Meta.Purity = FMath::Clamp(Cell.State.Meta.Purity - Burst, 0.0f, 1.0f);
        Cell.State.Meta.Stability = FMath::Clamp(Cell.State.Meta.Stability - Burst, 0.0f, 1.0f);
    }

    bSoloveyTriggered = true;
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Solovey] Morok triggered at (%d,%d): AoE Purity/Stability radius %d, burst %.2f"),
        SoloveySite.X, SoloveySite.Y, Radius, Burst);
    return true;
}

void AGridWorldManager::CalmSolovey()
{
    if (bSoloveyCalmed) return;
    bSoloveyCalmed = true;
    UE_LOG(LogHerbalistWorld, Log, TEXT("[Solovey] Calmed permanently by плакун-трава -- Morok will never trigger again"));
}

void AGridWorldManager::ApplyKalinovMostFightCost(const FIntPoint& Cell)
{
    FGridCell* GridCell = GetCell(Cell.X, Cell.Y);
    if (!GridCell) return;

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float Cost = Settings ? Settings->KalinovMostFightCost : 0.3f;

    GridCell->State.Meta.Purity = FMath::Clamp(GridCell->State.Meta.Purity - Cost, 0.0f, 1.0f);
    GridCell->State.Meta.Stability = FMath::Clamp(GridCell->State.Meta.Stability - Cost, 0.0f, 1.0f);

    UE_LOG(LogHerbalistWorld, Log, TEXT("[KalinovMost] Fight chosen at (%d,%d): Purity/Stability cost %.2f"),
        Cell.X, Cell.Y, Cost);
}

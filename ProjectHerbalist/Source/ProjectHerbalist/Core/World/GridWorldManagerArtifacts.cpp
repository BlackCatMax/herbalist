// Core/World/GridWorldManagerArtifacts.cpp
//
// Артефакты Легендарных сущностей (02_GDD/21_Journey_And_Artifacts.md
// §21.3-21.4, 2026-09-01, ревизия "Ending and artifacts"). Дубинка (Дубыня)
// изъята из дизайна этой ревизией — Смешанный лес теперь Баба-Яга/
// Шапка-невидимка (см. GridWorldManagerBases.cpp/LegendaryEntityTypes.h).
// Восемь эффектов §21.3 переписаны с "модификатора клетки" на "действие
// игрока", реализованы отдельными Exec-командами на контроллере, не здесь
// — этот файл остаётся общей инфраструктурой (реестр + проверка
// проявленности + честный/обманный путь получения), не местом для эффектов.

#include "Core/World/GridWorldManager.h"
#include "Core/Config/HerbalistSettings.h"
#include "Core/Simulation/Private/PerceptionService.h"
#include "Core/Entities/LegendaryEntityActor.h"
#include "Player/HerbalistPlayerController.h"
#include "ProjectHerbalist.h"
#include "HerbalistLogChannels.h"

bool AGridWorldManager::IsArtifactWarmed(FName ArtifactID) const
{
    const UHerbalistSettings* Settings = GetHerbalistSettings();

    // Фонарь — исключение (§21.4): греется от общей GlobalPerceptionClarity,
    // не от Warmth/зелья/региона. Не требует записи в AcquiredArtifacts для
    // этой проверки саму по себе, но в игре недостижимо без предварительной
    // честной... то есть обманной добычи (bDeceptionOnly) — проверка ниже
    // остаётся честной сама по себе, вызывающая сторона уже гарантирует
    // добычу перед использованием эффекта.
    if (ArtifactID == FName(TEXT("Фонарь")))
    {
        const float ClarityThreshold = Settings ? Settings->LanternWarmClarityThreshold : 0.7f;
        return GlobalPerceptionClarity >= ClarityThreshold;
    }

    const float WarmthThreshold = Settings ? Settings->ArtifactWarmthThreshold : 1.0f;
    for (const FAcquiredArtifact& Artifact : AcquiredArtifacts)
    {
        if (Artifact.ArtifactID == ArtifactID)
        {
            return Artifact.Warmth >= WarmthThreshold;
        }
    }
    return false;
}

bool AGridWorldManager::IsLegendaryManifested(FName EntityID) const
{
    if (const FIntPoint* Anchor = LegendaryAnchors.Find(EntityID))
    {
        const FGridCell* Cell = GetCellConst(Anchor->X, Anchor->Y);
        return Cell && Cell->ManifestedEntityID == EntityID;
    }
    // 2026-09-02 (унификация Берегини) -- нет якоря значит per-клеточная
    // карточка (bUsesCellHistoryPurity=true), у неё никогда не было
    // фиксированной клетки: сканируем все (было отдельным методом
    // IsBereginyaManifested(), поглощено сюда).
    for (const FGridCell& Cell : Cells)
    {
        if (Cell.ManifestedEntityID == EntityID) return true;
    }
    return false;
}

bool AGridWorldManager::TryAcquireArtifact(FName ArtifactID, const TArray<FInventoryItem>& Offered, bool& bOutViaDeception)
{
    bOutViaDeception = false;
    if (Offered.Num() == 0) return false;

    const FArtifactDefinition* Def = FindArtifactDefinition(ArtifactID);
    if (!Def) return false;

    // Уже добыт — не повторно (Зеркальце/Клубочек теперь тоже в этом
    // списке, см. комментарий у AcquiredArtifacts.Add ниже).
    for (const FAcquiredArtifact& Existing : AcquiredArtifacts)
    {
        if (Existing.ArtifactID == ArtifactID) return false;
    }

    // Доступен только когда сущность уже проявлена (§21.3). 2026-09-02
    // (унификация Берегини): раньше Гребень шёл через отдельную ветку
    // (Берегиня не в LegendaryEntityTypes.h) -- теперь она обычная строка
    // реестра, IsLegendaryManifested сама умеет её (fallback-скан без
    // якоря), развилка не нужна.
    if (!IsLegendaryManifested(Def->LegendaryEntityID)) return false;

    // Честный/обманный путь — то же различие S_real/S_Perceived, что уже
    // отличает тултип (AlchemySlotWidget.cpp) от настоящего Cell.State.
    // Собственный фиксированный сид на артефакт, не WorldRNG — наблюдение/
    // оценка подношения не должна возмущать детерминированный поток
    // симуляции (тот же приём, что уже MirrorPerceptionRng/PerceptionRng).
    FRandomStream Rng(20260901 + GetTypeHash(ArtifactID));
    float SumRealPurity = 0.0f;
    float SumPerceivedPurity = 0.0f;
    for (const FInventoryItem& Item : Offered)
    {
        SumRealPurity += Item.State.Meta.Purity;
        SumPerceivedPurity += Simulation::FPerceptionService::PerceiveRealState(Item.State, Rng, GlobalPerceptionClarity).Meta.Purity;
    }
    const float AvgRealPurity = SumRealPurity / Offered.Num();
    const float AvgPerceivedPurity = SumPerceivedPurity / Offered.Num();

    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const float HonestThreshold = Settings ? Settings->ArtifactHonestPurityThreshold : 0.6f;

    if (Def->bDeceptionOnly)
    {
        // Фонарь — единственный артефакт только через обман (§21.3: Болото
        // не держит благого Легендарного, честного пути к нему нет вовсе).
        // Настоящий Purity подношения не открывает отдельный честный путь —
        // только воспринятый.
        if (AvgPerceivedPurity < HonestThreshold) return false;
        bOutViaDeception = true;
    }
    else if (AvgRealPurity >= HonestThreshold)
    {
        bOutViaDeception = false;
    }
    else if (AvgPerceivedPurity >= HonestThreshold)
    {
        bOutViaDeception = true;
    }
    else
    {
        return false;   // ни честно, ни обманом — подношение просто недостаточно
    }

    // §21.2 (ревизия "Update docs", 2026-09-01): Зеркальце/Клубочек больше
    // не особый случай — "работают по общему правилу §21.3" — тоже
    // получают запись в AcquiredArtifacts (нужна для Warmth/прогрева §21.4,
    // тот же механизм, что у остальных шести). bWarmsCompanionItem теперь
    // значит только "вызывающая сторона (OfferForArtifact) дополнительно
    // выставляет bHasMirror/bHasYarnBall на контроллере", не "пропустить
    // запись здесь".
    FAcquiredArtifact Acquired;
    Acquired.ArtifactID = ArtifactID;
    Acquired.bAcquiredViaDeception = bOutViaDeception;
    AcquiredArtifacts.Add(Acquired);

    // НИТЬ МАТЕРИ (17_Hero_And_Community.md §17.7, Степь, событийный
    // триггер YarnBallAcquired) — Клубочек получен, "второй биом
    // путешествия". Доставляется напрямую здесь, тем же приёмом, что уже
    // BuyanPathChosen/CoherentBrew — не через периодический опрос.
    if (ArtifactID == FName(TEXT("Клубочек")))
    {
        AHerbalistPlayerController* PC = Cast<AHerbalistPlayerController>(GetWorld() ? GetWorld()->GetFirstPlayerController() : nullptr);
        CollectMemoryFragment(FName(TEXT("NIT_MATERI")), /*bIsFalse=*/false, PC);
    }

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] %s acquired (%s), RealPurity=%.2f, PerceivedPurity=%.2f"),
        *ArtifactID.ToString(), bOutViaDeception ? TEXT("via deception") : TEXT("honestly"),
        AvgRealPurity, AvgPerceivedPurity);
    return true;
}

bool AGridWorldManager::TryLureSwampTsarWithPotion(const FIntPoint& Cell, const FRealState& PotionState, bool& bOutGranted)
{
    bOutGranted = false;

    static const FName LanternID(TEXT("Фонарь"));
    static const FName TsarID(TEXT("Болотный царь"));
    static const FName CapID(TEXT("Шапка-невидимка"));

    // Уже добыт -- нечего красть повторно (тот же дубликат-гейт, что уже
    // TryAcquireArtifact выше).
    const bool bAlreadyAcquired = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == LanternID; });
    if (bAlreadyAcquired) return false;

    // Царь должен быть проявлен (Malign-спайк, §16.4) -- тот же гейт, что
    // уже TryAcquireArtifact применяет к честному/обманному подношению.
    if (!IsLegendaryManifested(TsarID)) return false;

    const FIntPoint* Anchor = LegendaryAnchors.Find(TsarID);
    if (!Anchor) return false;

    // "рядом с... Царём" -- Chebyshev-соседство с его якорной клеткой (та
    // же метрика расстояния, что уже применяет влияние хозяев места в
    // GetZaryanaPerceivedState), не обязательно ровно его клетка.
    const UHerbalistSettings* Settings = GetHerbalistSettings();
    const int32 Radius = Settings ? Settings->LurePotionRadius : 1;
    const int32 Dist = FMath::Max(FMath::Abs(Cell.X - Anchor->X), FMath::Abs(Cell.Y - Anchor->Y));
    if (Dist > Radius) return false;

    // Воспринятая Purity приманки -- тот же принцип S_real/S_Perceived, что
    // уже отличает честный/обманный путь выше, свой фиксированный сид (не
    // WorldRNG) — наблюдение/оценка приманки не должно возмущать
    // детерминированный поток симуляции, тот же довод, что уже у Rng в
    // TryAcquireArtifact.
    FRandomStream PerceptionRng(20260902 + GetTypeHash(Cell));
    const float PerceivedPurity = Simulation::FPerceptionService::PerceiveRealState(PotionState, PerceptionRng, GlobalPerceptionClarity).Meta.Purity;

    const float HonestThreshold = Settings ? Settings->ArtifactHonestPurityThreshold : 0.6f;
    if (PerceivedPurity < HonestThreshold)
    {
        // Приманка недостаточно убедительна -- попытка состоялась (зелье
        // расходуется вызывающей стороной), но Царь её не покупает.
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Lure potion at (%d,%d) unconvincing, PerceivedPurity=%.2f"),
            Cell.X, Cell.Y, PerceivedPurity);
        return true;
    }

    // Болотный царь, флагман (DESIGN_Entity_Actors_Art.md §4.4, 2026-09-06):
    // "водная гладь идёт кругами... признак, что Царь услышал и отвлёкся
    // на приманку" -- убедительная приманка сама по себе, независимо от
    // исхода броска ниже (Anchor уже проверен на существование строкой
    // выше через IsLegendaryManifested, актор на его клетке гарантированно
    // жив в этот момент).
    if (FGridCell* AnchorCellPtr = GetCell(Anchor->X, Anchor->Y))
    {
        if (ALegendaryEntityActor* TsarActor = Cast<ALegendaryEntityActor>(AnchorCellPtr->ManifestedEntityActor.Get()))
        {
            TsarActor->OnNoticedByBait();
        }
    }

    // Шапка-невидимка (§21.3, "необязательный ускоритель, не ключ") --
    // владение гарантирует исход вместо броска, но НЕ снимает требование
    // убедительной приманки выше: иначе Шапка стала бы отдельным, более
    // коротким путём к Фонарю в обход приманки вовсе, а не мостиком между
    // двумя артефактами -- "без Шапки путь всё равно доступен через одни
    // приманки" осталось бы верным лишь формально.
    const bool bHasCap = AcquiredArtifacts.ContainsByPredicate(
        [](const FAcquiredArtifact& A) { return A.ArtifactID == CapID; });

    bool bSuccess = bHasCap;
    if (!bHasCap)
    {
        // По-настоящему вероятностный исход (§21.3 "Сцена обмана") -- в
        // отличие от детерминированного порога TryAcquireArtifact, шанс
        // растёт линейно от порога убедительности (0%) до предельно
        // убедительной приманки (100%). WorldRNG (не PerceptionRng выше) --
        // настоящий игровой бросок, тот же принцип, что уже применяет
        // SpawnMemoryFragmentAt к риску ложного фрагмента (WorldRNG.FRand()).
        const float Chance = FMath::Clamp(
            (PerceivedPurity - HonestThreshold) / FMath::Max(1.0f - HonestThreshold, KINDA_SMALL_NUMBER), 0.0f, 1.0f);
        bSuccess = WorldRNG.FRand() < Chance;
    }

    if (!bSuccess)
    {
        UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Lure potion at (%d,%d) convincing (PerceivedPurity=%.2f) but the roll failed"),
            Cell.X, Cell.Y, PerceivedPurity);
        return true;
    }

    FAcquiredArtifact Acquired;
    Acquired.ArtifactID = LanternID;
    Acquired.bAcquiredViaDeception = true;
    AcquiredArtifacts.Add(Acquired);
    bOutGranted = true;

    UE_LOG(LogHerbalistWorld, Log, TEXT("[Artifact] Фонарь stolen from Болотный царь via lure potion at (%d,%d), PerceivedPurity=%.2f%s"),
        Cell.X, Cell.Y, PerceivedPurity, bHasCap ? TEXT(" (Шапка guaranteed)") : TEXT(""));
    return true;
}

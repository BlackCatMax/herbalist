// Source/ProjectHerbalistTests/Private/Tests/TraceReplayTest.cpp
//
// Полный аудит проекта (2026-08-31, продолжение по прямому запросу
// пользователя): Perception/TraceReplay был назван крупнейшим оставшимся
// тестовым пробелом в ROADMAP.md — заявленный, но не автоматически
// проверяемый столп детерминизма. При ближайшем чтении оказался НЕ
// архитектурно сложной задачей, как предполагала ROADMAP: `FTraceFrame`
// (TraceTypes.h) и `Simulation::ReplayAndCompare` (TraceReplay.cpp) не
// завязаны ни на мир, ни на GameInstance вовсе — чистые структуры и
// свободная функция, тот же вертикальный срез, что и остальной пайплайн
// в PipelineV2Test.cpp/GatheringToolTest.cpp.
//
// Что уже покрыто в другом месте, не дублируется здесь:
// `ProjectHerbalist.PipelineV2.ApplyIsDeterministic` уже доказывает, что
// сам ExecutePipeline на одном сиде даёт побитово одинаковый результат.
// Этот файл проверяет другое — саму машинерию верификации-через-повтор
// (TraceReplay.cpp), которая в проде используется для отладки/регрессии,
// а не переоткрывает уже доказанное свойство пайплайна.

#include "Core/Simulation/Public/SnapshotTypes.h"
#include "Core/Simulation/Public/DeltaTypes.h"
#include "Core/Simulation/Public/CommandTypes.h"
#include "Core/Simulation/Public/TraceTypes.h"
#include "PipelineV2.h"
#include "TraceReplay.h"
#include "Core/Types/HerbalistCoreTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    FGridCell TraceTestNeutralCell()
    {
        FGridCell Cell;
        Cell.X = 3; Cell.Y = 3;
        Cell.Biome = EBiomeType::MixedForest;
        Cell.bIsWater = false;
        Cell.State.Magnitude = 0.5f;
        Cell.State.Direction.Body = 0.25f; Cell.State.Direction.Mind = 0.25f;
        Cell.State.Direction.Spirit = 0.25f; Cell.State.Direction.Nature = 0.25f;
        Cell.State.Meta.Purity = 0.5f; Cell.State.Meta.Corruption = 0.2f;
        Cell.State.Meta.Distortion = 0.2f; Cell.State.Meta.Stability = 0.5f;
        Cell.State.Meta.Potency = 0.5f; Cell.State.Meta.Resonance = 0.5f;
        return Cell;
    }

    // Собирает FTraceFrame ровно тем же способом, что и продакшн-код
    // (AGridWorldManager::RunSimulationStep -> TraceBuffer.Record, см.
    // GridWorldManagerTick.cpp) — снапшот+команды на входе, дельта на
    // выходе, один реальный прогон ExecutePipeline.
    FTraceFrame CaptureHarvestFrame(int32 Seed, FName IngredientID = TEXT("TraceProbe"))
    {
        const FGridCell Cell = TraceTestNeutralCell();

        FTraceFrame Frame;
        Frame.TickID = 7;
        Frame.WorldSnapshot.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
        Frame.WorldSnapshot.WorldSeed = Seed;

        FInventorySnapshot InvSnap;

        FCommandEntry CmdEntry;
        CmdEntry.Primitive = ECommandPrimitive::Harvest;
        CmdEntry.Harvest.TargetCell = FIntPoint(Cell.X, Cell.Y);
        CmdEntry.Harvest.IngredientID = IngredientID;
        CmdEntry.Harvest.Amount = 1;
        CmdEntry.Harvest.BaseState = Cell.State;
        CmdEntry.Harvest.Resilience = 1.0f;
        Frame.Commands.Add(CmdEntry);

        FCommandBatch Batch;
        Batch.Commands = Frame.Commands;

        FRandomStream Rng(Seed);
        Frame.GeneratedDelta = Simulation::ExecutePipeline(Frame.WorldSnapshot, InvSnap, Frame.BiomeSnapshot, Batch, Rng);
        return Frame;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_ConfirmsIdenticalRerun,
    "Herbalist.TraceReplay.ConfirmsIdenticalRerun",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_ConfirmsIdenticalRerun::RunTest(const FString& Parameters)
{
    const int32 Seed = 9001;
    const FTraceFrame Frame = CaptureHarvestFrame(Seed);
    TestTrue(TEXT("A real harvest produced at least one world change to compare"), Frame.GeneratedDelta.WorldChanges.Num() > 0);

    // Тот же сид, что при захвате кадра -- повтор обязан дать побитово
    // тот же результат (заявленный архитектурный столп проекта).
    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestTrue(TEXT("ReplayAndCompare confirms a genuinely deterministic rerun"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_CatchesWorldChangeCountMismatch,
    "Herbalist.TraceReplay.CatchesWorldChangeCountMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_CatchesWorldChangeCountMismatch::RunTest(const FString& Parameters)
{
    // Не только "совпадающие кадры проходят" -- сама проверка обязана
    // ловить расхождение, иначе она ничего не значит. Подделываем
    // GeneratedDelta так, будто исходный прогон видел на одну изменённую
    // клетку больше, чем повтор реально даст.
    const int32 Seed = 9002;
    FTraceFrame Frame = CaptureHarvestFrame(Seed);
    Frame.GeneratedDelta.WorldChanges.Add(FIntPoint(99, 99), TraceTestNeutralCell());

    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestFalse(TEXT("A tampered WorldChanges count is caught, not silently accepted"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_CatchesCellStateMismatch,
    "Herbalist.TraceReplay.CatchesCellStateMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_CatchesCellStateMismatch::RunTest(const FString& Parameters)
{
    // Тоньше предыдущего теста -- то же количество изменённых клеток
    // (проходит первую проверку по Num()), но значение внутри одной из
    // них подделано. Именно эта ветка -- то, что реально ловит "пайплайн
    // на этом сиде дал другой результат", не просто "что-то пропало".
    const int32 Seed = 9003;
    FTraceFrame Frame = CaptureHarvestFrame(Seed);
    if (!TestTrue(TEXT("Precondition: at least one world change to tamper with"), Frame.GeneratedDelta.WorldChanges.Num() > 0))
    {
        return false;
    }

    for (auto& Pair : Frame.GeneratedDelta.WorldChanges)
    {
        Pair.Value.State.Magnitude = FMath::Clamp(Pair.Value.State.Magnitude + 0.5f, 0.0f, 1.0f);
        if (FMath::IsNearlyEqual(Pair.Value.State.Magnitude, Pair.Value.State.Magnitude - 0.5f))
        {
            // На случай если +0.5 не изменило значение из-за клампа у
            // самого потолка -- сдвигаем в другую сторону, тест не должен
            // зависеть от конкретного случайного джиттера сбора.
            Pair.Value.State.Magnitude = FMath::Clamp(Pair.Value.State.Magnitude - 0.9f, 0.0f, 1.0f);
        }
    }

    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestFalse(TEXT("A tampered cell Magnitude is caught, not silently accepted"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_CatchesTargetStateNudgeMismatch,
    "Herbalist.TraceReplay.CatchesTargetStateNudgeMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_CatchesTargetStateNudgeMismatch::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05: раньше TargetStateNudges вообще не сравнивался --
    // подделка здесь молча проходила бы как SUCCESS. Тот же приём
    // подделки, что уже CatchesWorldChangeCountMismatch применяет к
    // WorldChanges (харвест-команда сама по себе не трогает
    // TargetStateNudges, значит и реальный повтор даст здесь 0 записей).
    const int32 Seed = 9006;
    FTraceFrame Frame = CaptureHarvestFrame(Seed);
    Frame.GeneratedDelta.TargetStateNudges.Add(FIntPoint(50, 50), FRealState());

    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestFalse(TEXT("A tampered TargetStateNudges count is caught, not silently accepted"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_CatchesBiomeActivationsMismatch,
    "Herbalist.TraceReplay.CatchesBiomeActivationsMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_CatchesBiomeActivationsMismatch::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05: BiomeActivations тоже не сравнивался вовсе.
    const int32 Seed = 9007;
    FTraceFrame Frame = CaptureHarvestFrame(Seed);
    Frame.GeneratedDelta.BiomeActivations.Add(FName(TEXT("TamperedBiome")));

    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestFalse(TEXT("A tampered BiomeActivations count is caught, not silently accepted"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_CatchesFootprintMismatch,
    "Herbalist.TraceReplay.CatchesFootprintMismatch",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_CatchesFootprintMismatch::RunTest(const FString& Parameters)
{
    // Аудит 2026-09-05: Footprints тоже не сравнивался вовсе.
    const int32 Seed = 9008;
    FTraceFrame Frame = CaptureHarvestFrame(Seed);
    FBiomeFootprintEntry FakeFootprint;
    FakeFootprint.BiomeID = FName(TEXT("TamperedBiome"));
    FakeFootprint.MorokImpact = 0.5f;
    Frame.GeneratedDelta.Footprints.Add(FakeFootprint);

    FRandomStream ReplayRng(Seed);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, ReplayRng);

    TestFalse(TEXT("A tampered Footprints count is caught, not silently accepted"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_DifferentSeedProducesDifferentResultAndIsCaught,
    "Herbalist.TraceReplay.DifferentSeedProducesDifferentResultAndIsCaught",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_DifferentSeedProducesDifferentResultAndIsCaught::RunTest(const FString& Parameters)
{
    // Реалистичный failure mode, не синтетическая порча: кадр захвачен на
    // сиде A, но повтор по ошибке идёт с сидом B (например, баг в том, что
    // сохраняется в трассировку). ReplayAndCompare обязан НЕ соврать, что
    // всё сошлось.
    const FTraceFrame Frame = CaptureHarvestFrame(9004);

    FRandomStream WrongSeedRng(9005);
    const bool bMatches = Simulation::ReplayAndCompare(Frame, WrongSeedRng);

    TestFalse(TEXT("Replaying with the wrong seed does not falsely report determinism"), bMatches);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTraceReplay_RingBufferRecordsAndRetrievesLastFrame,
    "Herbalist.TraceReplay.RingBufferRecordsAndRetrievesLastFrame",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTraceReplay_RingBufferRecordsAndRetrievesLastFrame::RunTest(const FString& Parameters)
{
    FTraceRingBuffer Buffer;
    TestNull(TEXT("Empty buffer has no last frame"), Buffer.GetLastFrame());

    const FGridCell Cell = TraceTestNeutralCell();
    FWorldSnapshot WorldSnap;
    WorldSnap.GridState.Add(FIntPoint(Cell.X, Cell.Y), Cell);
    FBiomeSnapshot BiomeSnap;
    TArray<FCommandEntry> Commands;
    FStateDelta Delta;
    Delta.WorldChanges.Add(FIntPoint(Cell.X, Cell.Y), Cell);

    for (int32 i = 0; i < 5; ++i)
    {
        Buffer.Record(i, WorldSnap, BiomeSnap, Commands, Delta);
    }

    const FTraceFrame* Last = Buffer.GetLastFrame();
    if (!TestNotNull(TEXT("Last frame available after recording"), Last)) return false;
    TestEqual(TEXT("Last frame has the most recently recorded TickID"), Last->TickID, 4);

    // Кольцевой буфер -- переполнение не должно ни расти безгранично, ни
    // указывать на пустую/некорректную запись.
    for (int32 i = 5; i < FTraceRingBuffer::MaxFrames + 10; ++i)
    {
        Buffer.Record(i, WorldSnap, BiomeSnap, Commands, Delta);
    }
    TestEqual(TEXT("Buffer never grows past MaxFrames"), Buffer.Frames.Num(), FTraceRingBuffer::MaxFrames);
    const FTraceFrame* LastAfterWrap = Buffer.GetLastFrame();
    if (TestNotNull(TEXT("Last frame still available after wraparound"), LastAfterWrap))
    {
        TestEqual(TEXT("Last frame after wraparound is the most recent TickID, not a stale slot"),
            LastAfterWrap->TickID, FTraceRingBuffer::MaxFrames + 9);
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS

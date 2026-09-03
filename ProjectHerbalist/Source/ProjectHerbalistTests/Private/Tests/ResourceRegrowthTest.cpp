// Source/ProjectHerbalistTests/Private/Tests/ResourceRegrowthTest.cpp
//
// Отрастание поресурсно, не по клетке (2026-09-04, прямой запрос
// пользователя: "а можно отрастание сделать поресурсно, а не по клеткам?").
//
// До этой правки AGridWorldManager::OnResourceCollected звал
// StartRegeneration только когда Cell.ResourceActors пустел ДО НУЛЯ, и
// тогда StartRegeneration отращивал целую новую пачку (WorldRNG.RandRange
// заново). Клетка с несколькими ресурсами (MinResourcesPerCell региона
// поднят выше дефолтных 1-3 -- ровно то, что показал лог пользователя:
// "Cell (3,4): ресурсов 123") не отращивала НИЧЕГО, пока не соберут
// буквально всё до последнего. Теперь один харвест = один собственный
// таймер на один новый ресурс, независимо от соседей по клетке.
//
// Наблюдаем это через FGridCell::PendingRegrowthCount -- счётчик,
// заведённый этой же правкой специально для того, чтобы поведение можно
// было проверить синхронно, не дожидаясь реального срабатывания таймера
// (5-10 минут игрового времени -- на автотесте это заведомо дольше, чем
// вообще существует тестовый мир, см. TestWorldHelpers.h). Инкремент
// происходит СИНХРОННО внутри StartRegeneration, до постановки таймера --
// этого достаточно, чтобы отличить "запланировано" от "не запланировано",
// без необходимости прокручивать сам таймер.

#include "Core/World/GridWorldManager.h"
#include "Core/World/BiomeRegionVolume.h"
#include "Core/Resources/AHerbalistResourceActor.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    AHerbalistResourceActor* SpawnResourceAt(UWorld* World, AGridWorldManager* Manager, int32 X, int32 Y)
    {
        AHerbalistResourceActor* Resource = World->SpawnActor<AHerbalistResourceActor>();
        if (Resource)
        {
            Resource->Init(FName(TEXT("bol_12")), FText::FromString(TEXT("Зверобой")), nullptr,
                FRealState(), FVector::ZeroVector, Manager, X, Y, 0.0f, false, false);
        }
        return Resource;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceRegrowth_HarvestingOneOfManyStartsRegrowthImmediately,
    "Herbalist.ResourceRegrowth.HarvestingOneOfManyStartsRegrowthImmediately",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceRegrowth_HarvestingOneOfManyStartsRegrowthImmediately::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    AHerbalistResourceActor* R0 = SpawnResourceAt(World, Manager, 5, 5);
    AHerbalistResourceActor* R1 = SpawnResourceAt(World, Manager, 5, 5);
    AHerbalistResourceActor* R2 = SpawnResourceAt(World, Manager, 5, 5);
    if (!TestNotNull(TEXT("R0"), R0) || !TestNotNull(TEXT("R1"), R1) || !TestNotNull(TEXT("R2"), R2))
    {
        Manager->Destroy();
        return false;
    }

    FGridCell* Cell = Manager->GetCell(5, 5);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }
    if (!TestEqual(TEXT("Все три ресурса зарегистрированы в клетке"), Cell->ResourceActors.Num(), 3))
    {
        Manager->Destroy();
        return false;
    }

    // Собираем ОДИН из трёх -- под старой (клеточной) логикой это не
    // запустило бы отрастание вовсе: Cell->ResourceActors.Num() стал бы 2,
    // не 0, и гейт "== 0" молчал бы, пока не соберут оставшиеся два.
    Manager->OnResourceCollected(R0);

    TestEqual(TEXT("Собранный убран, два оставшихся ресурса не тронуты"), Cell->ResourceActors.Num(), 2);
    TestEqual(TEXT("Отрастание запланировано СРАЗУ на один собранный слот, не отложено до опустошения клетки"),
        Cell->PendingRegrowthCount, 1);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceRegrowth_EachHarvestGetsItsOwnIndependentSlot,
    "Herbalist.ResourceRegrowth.EachHarvestGetsItsOwnIndependentSlot",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceRegrowth_EachHarvestGetsItsOwnIndependentSlot::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    AHerbalistResourceActor* R0 = SpawnResourceAt(World, Manager, 6, 6);
    AHerbalistResourceActor* R1 = SpawnResourceAt(World, Manager, 6, 6);
    if (!TestNotNull(TEXT("R0"), R0) || !TestNotNull(TEXT("R1"), R1)) { Manager->Destroy(); return false; }

    FGridCell* Cell = Manager->GetCell(6, 6);
    if (!TestNotNull(TEXT("Cell exists"), Cell)) { Manager->Destroy(); return false; }

    // Два харвеста подряд -- два НЕЗАВИСИМЫХ таймера, не один общий на
    // клетку (старый локальный, ничем не считаемый FTimerHandle внутри
    // StartRegeneration не давал это проверить в принципе -- второй вызов
    // тихо перезаписывал бы единственную переменную первого).
    Manager->OnResourceCollected(R0);
    Manager->OnResourceCollected(R1);

    TestEqual(TEXT("Клетка пуста"), Cell->ResourceActors.Num(), 0);
    TestEqual(TEXT("Оба харвеста учтены как два раздельных ожидающих слота"), Cell->PendingRegrowthCount, 2);

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistResourceRegrowth_DefaultTimeIsInRequestedMinutesRange,
    "Herbalist.ResourceRegrowth.DefaultTimeIsInRequestedMinutesRange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistResourceRegrowth_DefaultTimeIsInRequestedMinutesRange::RunTest(const FString& Parameters)
{
    // "дефолт побольше, минут 5-10" (2026-09-04) -- старое значение (10.0f)
    // было секундами, отладочным пережитком, из-за которого отрастание в
    // PIE выглядело почти мгновенным. Числовой диапазон запросил сам
    // пользователь -- тест проверяет попадание в него, не конкретное число,
    // чтобы точная настройка внутри 5-10 минут осталась игровым решением,
    // а не тестовым контрактом.
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = World->SpawnActor<AGridWorldManager>();
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    ABiomeRegionVolume* Region = World->SpawnActor<ABiomeRegionVolume>();
    if (!TestNotNull(TEXT("Region spawned"), Region)) { Manager->Destroy(); return false; }

    TestTrue(TEXT("AGridWorldManager::ResourceRegrowthTime в пределах 5-10 минут"),
        Manager->ResourceRegrowthTime >= 300.0f && Manager->ResourceRegrowthTime <= 600.0f);
    TestTrue(TEXT("ABiomeRegionVolume::ResourceRegrowthTimeSeconds в пределах 5-10 минут"),
        Region->ResourceRegrowthTimeSeconds >= 300.0f && Region->ResourceRegrowthTimeSeconds <= 600.0f);

    Manager->Destroy();
    Region->Destroy();
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR

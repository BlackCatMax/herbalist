// Source/ProjectHerbalistTests/Private/Tests/EntitySpacingTest.cpp
//
// Минимальная дистанция между проявлениями одного вида (2026-09-03,
// жалоба пользователя: "как уменьшить плотность существ? слишком много...
// спавнятся буквально каждые 3 метра").
//
// До этого плотность существ нечем было регулировать вовсе: потолок
// задавала сетка -- одно поле Cell.ManifestedEntityID, максимум одна
// сущность на клетку, то есть одна на CellSize. Порог (TriggerThreshold)
// отвечает за "может ли здесь появиться", но не за "как редко" -- поднимая
// его, получаешь те же плотные заросли на меньшей площади.
//
// Тестируется IsCrowdedBySameEntity напрямую: определения приходят из
// боевой DT_AmbientEntities через function-local static кэш
// (GetAmbientEntityDefinitions), подменить их в тесте нечем -- а вот
// собрать свой FAmbientEntityDefinition и передать его сюда можно.

#include "Core/World/GridWorldManager.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Types/BiomeTypes.h"
#include "Core/Config/HerbalistSettings.h"
#include "Misc/AutomationTest.h"
#include "Editor.h"
#include "Engine/World.h"

#if WITH_AUTOMATION_TESTS && WITH_EDITOR

#include "TestWorldHelpers.h"

namespace
{
    FAmbientEntityDefinition MakeSpacedDefinition(FName EntityID, float SpacingMeters)
    {
        FAmbientEntityDefinition Def;
        Def.EntityID = EntityID;
        Def.Biome = EBiomeType::Taiga;
        Def.MinSpacingMeters = SpacingMeters;
        return Def;
    }

    // Настройка живёт в CDO и переживает тест -- обязательно возвращаем,
    // иначе следующие тесты поедут в зависимости от порядка выполнения
    // (тот же приём и тот же довод, что FScopedPlacementSettings в
    // SpawnPlacementTest.cpp).
    struct FScopedSpacingMultiplier
    {
        UHerbalistSettings* Settings;
        float Saved;

        explicit FScopedSpacingMultiplier(float Value)
            : Settings(GetMutableDefault<UHerbalistSettings>())
            , Saved(Settings->EntitySpacingMultiplier)
        {
            Settings->EntitySpacingMultiplier = Value;
        }
        ~FScopedSpacingMultiplier()
        {
            Settings->EntitySpacingMultiplier = Saved;
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_NeighbourOfSameSpeciesCrowdsOutANewOne,
    "Herbalist.EntitySpacing.NeighbourOfSameSpeciesCrowdsOutANewOne",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_NeighbourOfSameSpeciesCrowdsOutANewOne::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // CellSize=100 в тестовом мире -- клетка 1 м, значит 3 м дистанции это
    // радиус ровно 3 клетки. Числа подобраны так, чтобы граница круга
    // проверялась явно, а не "где-то там".
    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 3.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* CloseNeighbour = Manager->GetCell(12, 10);   // 2 м -- внутри дистанции
    FGridCell* FarNeighbour = Manager->GetCell(15, 10);     // 5 м -- за дистанцией
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !CloseNeighbour || !FarNeighbour)
    {
        Manager->Destroy();
        return false;
    }

    // Пусто вокруг -- никто не мешает.
    TestFalse(TEXT("Nothing manifested nearby -- not crowded"), Manager->IsCrowdedBySameEntity(*Candidate, Def));

    // Свой вид в двух метрах -- мешает.
    CloseNeighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));
    TestTrue(TEXT("Same species two metres away crowds the candidate out"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    // Тот же сосед, но за пределами дистанции -- не мешает.
    CloseNeighbour->ManifestedEntityID = NAME_None;
    FarNeighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));
    TestFalse(TEXT("Same species five metres away (beyond 3 m) does not crowd"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_DifferentSpeciesDoNotCrowdEachOther,
    "Herbalist.EntitySpacing.DifferentSpeciesDoNotCrowdEachOther",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_DifferentSpeciesDoNotCrowdEachOther::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Прямое пожелание пользователя: "Гнильники могут кишеть, а Лихо
    // встречается раз на километр" -- дистанция считается ПО ВИДУ, виды не
    // расталкивают друг друга.
    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 5.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* Neighbour = Manager->GetCell(11, 10);
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !Neighbour) { Manager->Destroy(); return false; }

    Neighbour->ManifestedEntityID = FName(TEXT("SomeoneElse"));
    TestFalse(TEXT("A different species right next door does not crowd"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_ZeroSpacingMeansMechanismIsOff,
    "Herbalist.EntitySpacing.ZeroSpacingMeansMechanismIsOff",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_ZeroSpacingMeansMechanismIsOff::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Дефолт всех 33 боевых строк -- именно 0: механизм есть, но выключен,
    // пока вид не настроят руками. Регрессия на "не сломали старый мир".
    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 0.0f);
    TestEqual(TEXT("Struct default really is 0 (off)"), FAmbientEntityDefinition().MinSpacingMeters, 0.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* Neighbour = Manager->GetCell(11, 10);
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !Neighbour) { Manager->Destroy(); return false; }

    Neighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));
    TestFalse(TEXT("Spacing 0 -- even the immediate neighbour does not crowd"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    Manager->Destroy();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_SpacingIsMeasuredInMetresNotCells,
    "Herbalist.EntitySpacing.SpacingIsMeasuredInMetresNotCells",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_SpacingIsMeasuredInMetresNotCells::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Главное свойство выбора "метры, а не клетки" (обожглись на этом же
    // высотным поясом тем же днём): смена CellSize не должна менять
    // физическую дистанцию. Сосед в ОДНОЙ клетке при CellSize=100 стоит в
    // 1 м и попадает под дистанцию 3 м; при CellSize=1000 тот же сосед
    // стоит уже в 10 м и под неё не попадает.
    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 3.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* Neighbour = Manager->GetCell(11, 10);
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !Neighbour) { Manager->Destroy(); return false; }

    Neighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));

    Manager->CellSize = 100.0f;   // клетка 1 м -- сосед в 1 м, внутри 3 м
    TestTrue(TEXT("At 1 m cells, the adjacent neighbour is within 3 m"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    Manager->CellSize = 1000.0f;  // клетка 10 м -- тот же сосед уже в 10 м
    TestFalse(TEXT("At 10 m cells, the same adjacent neighbour is beyond 3 m"),
        Manager->IsCrowdedBySameEntity(*Candidate, Def));

    Manager->Destroy();
    return true;
}

// Ручка плотности заселения (2026-09-07, прямой запрос пользователя).
// Числа дистанций в карточках заданы для боевого масштаба (клетка 10 м);
// на дев-карте клетка -- 1 м, и те же 15 м дают радиус 15 клеток при сетке
// 20x20, то есть одну особь вида на весь мир. Множитель приводит масштабы
// друг к другу, не трогая данные карточек.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_MultiplierScalesTheEffectiveDistance,
    "Herbalist.EntitySpacing.MultiplierScalesTheEffectiveDistance",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_MultiplierScalesTheEffectiveDistance::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    // Карточка на 10 м; сосед стоит в 5 м (клетка = 1 м в тестовом мире).
    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 10.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* Neighbour = Manager->GetCell(15, 10);
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !Neighbour) { Manager->Destroy(); return false; }
    Neighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));

    {
        FScopedSpacingMultiplier Scope(1.0f);
        TestTrue(TEXT("При множителе 1.0 дистанция 10 м действует -- сосед в 5 м мешает"),
            Manager->IsCrowdedBySameEntity(*Candidate, Def));
    }
    {
        // 10 м * 0.4 = 4 м -- сосед в пяти метрах теперь снаружи круга.
        FScopedSpacingMultiplier Scope(0.4f);
        TestFalse(TEXT("При множителе 0.4 та же дистанция сжимается до 4 м -- сосед в 5 м больше не мешает"),
            Manager->IsCrowdedBySameEntity(*Candidate, Def));
    }
    {
        // 10 м * 2.0 = 20 м -- дальний сосед снова внутри.
        FScopedSpacingMultiplier Scope(2.0f);
        FGridCell* FarNeighbour = Manager->GetCell(10, 25);   // 15 м по Y
        if (FarNeighbour)
        {
            Neighbour->ManifestedEntityID = NAME_None;
            FarNeighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));
            TestTrue(TEXT("При множителе 2.0 дистанция растягивается до 20 м -- сосед в 15 м мешает"),
                Manager->IsCrowdedBySameEntity(*Candidate, Def));
            FarNeighbour->ManifestedEntityID = NAME_None;
        }
        Neighbour->ManifestedEntityID = FName(TEXT("TestSpirit"));
    }

    Manager->Destroy();
    return true;
}

// Ноль -- полное выключение разрежения. Нужен именно как отдельный режим:
// он позволяет отделить этот механизм (одинаковые виды, геометрия) от
// «занял -- держит вечно» в CanManifest (РАЗНЫЕ виды, приоритет ранга) --
// две совершенно разные причины, по которым существо может не проявиться,
// и путать их при отладке дорого.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_ZeroMultiplierDisablesCrowdingEntirely,
    "Herbalist.EntitySpacing.ZeroMultiplierDisablesCrowdingEntirely",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_ZeroMultiplierDisablesCrowdingEntirely::RunTest(const FString& Parameters)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!TestNotNull(TEXT("Editor world available"), World)) return false;

    AGridWorldManager* Manager = SpawnAndBeginPlay(World);
    if (!TestNotNull(TEXT("Manager spawned"), Manager)) return false;

    const FAmbientEntityDefinition Def = MakeSpacedDefinition(FName(TEXT("TestSpirit")), 30.0f);

    FGridCell* Candidate = Manager->GetCell(10, 10);
    FGridCell* Adjacent = Manager->GetCell(11, 10);   // вплотную, 1 м
    if (!TestNotNull(TEXT("Cells exist"), Candidate) || !Adjacent) { Manager->Destroy(); return false; }
    Adjacent->ManifestedEntityID = FName(TEXT("TestSpirit"));

    {
        FScopedSpacingMultiplier Scope(1.0f);
        TestTrue(TEXT("Контроль: при множителе 1.0 сосед вплотную мешает"),
            Manager->IsCrowdedBySameEntity(*Candidate, Def));
    }
    {
        FScopedSpacingMultiplier Scope(0.0f);
        TestFalse(TEXT("Множитель 0 выключает разрежение -- даже соседняя клетка не мешает"),
            Manager->IsCrowdedBySameEntity(*Candidate, Def));
    }

    Manager->Destroy();
    return true;
}

// Дефолт настройки обязан быть нейтральным: включение ручки не должно
// молча изменить мир у того, кто её не трогал.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistEntitySpacing_MultiplierDefaultsToNeutral,
    "Herbalist.EntitySpacing.MultiplierDefaultsToNeutral",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistEntitySpacing_MultiplierDefaultsToNeutral::RunTest(const FString& Parameters)
{
    const UHerbalistSettings* Defaults = GetDefault<UHerbalistSettings>();
    if (!TestNotNull(TEXT("Settings CDO available"), Defaults)) return false;

    TestEqual(TEXT("EntitySpacingMultiplier по умолчанию 1.0 -- поведение ровно как до появления ручки"),
        Defaults->EntitySpacingMultiplier, 1.0f);
    return true;
}

#endif // WITH_AUTOMATION_TESTS && WITH_EDITOR

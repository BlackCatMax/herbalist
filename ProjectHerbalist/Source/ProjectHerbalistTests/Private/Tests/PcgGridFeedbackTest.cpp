// Source/ProjectHerbalistTests/Private/Tests/PcgGridFeedbackTest.cpp
//
// Обратная связь «симуляция -> PCG-граф» (2026-09-03). Узел
// UPCGHerbalistGridSettings отдаёт графу сетку облаком точек с атрибутами
// состояния, чтобы растительность вырастала ИЗ симуляции, а не была
// покрашена поверх неё.
//
// Полноценно прогнать сам PCG-элемент в headless-тесте нельзя: ему нужен
// исполняющий контекст графа (FPCGContext с ExecutionSource), которого в
// голом editor-мире нет. Поэтому здесь держится то, что реально ломается
// молча и дорого: имена атрибутов -- это КОНТРАКТ с графом. Пользователь
// собирает ноды Attribute Filter по этим именам вручную; переименование в
// C++ не вызовет ни ошибки компиляции, ни падения теста -- просто в один
// день граф перестанет фильтровать, и причину будут искать долго.

#include "Core/PCG/PCGHerbalistGridData.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#if WITH_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgFeedback_NodeExposesTheDocumentedSwitches,
    "Herbalist.PcgFeedback.NodeExposesTheDocumentedSwitches",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgFeedback_NodeExposesTheDocumentedSwitches::RunTest(const FString& Parameters)
{
    UPCGHerbalistGridSettings* Settings = NewObject<UPCGHerbalistGridSettings>();
    if (!TestNotNull(TEXT("Node settings object created"), Settings)) return false;

    // Дефолты -- часть договорённости с пользователем: граф по умолчанию
    // получает только то, что реально симулируется и реально нарисовано
    // регионами, иначе он захлебнётся дальним миром, состояние которого
    // всё равно догоняется лениво и на момент запроса неактуально.
    TestTrue(TEXT("By default only actively simulated cells are handed to the graph"), Settings->bOnlyActiveCells);
    TestTrue(TEXT("By default only cells claimed by biome regions are handed over"), Settings->bOnlyCellsClaimedByBiomeRegions);
    TestFalse(TEXT("Water cells are included by default -- excluding them is opt-in"), Settings->bExcludeWaterCells);

    // Узел действительно наследует UPCGSettings и потому виден палитре
    // графа (CreateElement -- protected, как и у всех штатных узлов, снаружи
    // его дёргает сам фреймворк).
    TestTrue(TEXT("Node is a PCG settings type the graph can instantiate"),
        Settings->IsA<UPCGSettings>());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgFeedback_AttributeNamesAreAStableContract,
    "Herbalist.PcgFeedback.AttributeNamesAreAStableContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgFeedback_AttributeNamesAreAStableContract::RunTest(const FString& Parameters)
{
    // Список ровно тот, что узел кладёт в метаданные точки. Если кто-то
    // переименует атрибут в C++, этот тест упадёт и заставит осознанно
    // подтвердить, что уже собранные графы пользователя ломаются.
    const TArray<FString> Expected = {
        TEXT("Distortion"),
        TEXT("Corruption"),
        TEXT("Purity"),
        TEXT("Stability"),
        TEXT("HarvestStress"),
        TEXT("ShrineRestoration"),
        TEXT("Biome"),
        TEXT("bIsWater"),
        TEXT("ManifestedEntity"),
    };

    // Читаем то, что реально объявлено в .cpp узла: единственный способ не
    // продублировать список ещё раз и не дать ему разъехаться -- сравнить с
    // исходником. Тест намеренно завязан на файл, а не на копию списка.
    const FString SourcePath = FPaths::Combine(FPaths::ProjectDir(),
        TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistGridData.cpp"));

    FString Source;
    if (!TestTrue(TEXT("Node source file is readable"), FFileHelper::LoadFileToString(Source, *SourcePath)))
    {
        return false;
    }

    for (const FString& Name : Expected)
    {
        const FString Needle = FString::Printf(TEXT("TEXT(\"%s\")"), *Name);
        TestTrue(FString::Printf(TEXT("Attribute '%s' is still declared by the node"), *Name),
            Source.Contains(Needle));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS

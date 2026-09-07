// Source/ProjectHerbalistTests/Private/Tests/PcgSampleCellTest.cpp
//
// Узел «Sample Herbalist Cell» (2026-09-08) -- переносит состояние клетки на
// чужие точки и выдаёт им MeshKey для PCGMeshSelectorByAttribute.
//
// То же ограничение, что у PcgGridFeedbackTest.cpp: прогнать сам PCG-элемент
// в headless-тесте нельзя, ему нужен исполняющий контекст графа (FPCGContext
// с ExecutionSource), которого в голом editor-мире нет. Поэтому проверяется
// то, что ломается молча и дорого:
//
//   1. У узла ЕСТЬ входной пин. В этом весь смысл его существования: у
//      соседнего «Get Herbalist Grid» входных пинов нет вовсе, он источник и
//      пометить чужие точки не может. Если этот пин однажды исчезнет,
//      компиляция не заметит, а собранный граф просто развалится.
//   2. Имена атрибутов -- контракт с графом, который пользователь собирает
//      руками по этим именам.
//   3. Имена, общие с «Get Herbalist Grid», совпадают ПОБУКВЕННО. Два узла
//      кладут в точку одни и те же оси; разъедься они -- и Attribute Filter,
//      настроенный на выход одного, тихо перестал бы находить их на выходе
//      другого.

#include "Core/PCG/PCGHerbalistSampleCell.h"
#include "Core/PCG/PCGHerbalistGridData.h"
#include "PCGPin.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Своё имя, не SourceOf/LoadSource: unity-сборка склеивает все Tests/*.cpp
    // в одну единицу трансляции, и одинаковые тела в анонимных namespace дают
    // MSVC C2084 (наступали на это 2026-08-29, см. шапку TestWorldHelpers.h).
    bool LoadPcgSampleCellSource(const TCHAR* RelativePath, FString& OutSource)
    {
        const FString FullPath = FPaths::Combine(FPaths::ProjectDir(), RelativePath);
        return FFileHelper::LoadFileToString(OutSource, *FullPath);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_HasAnInputPinUnlikeTheSourceNode,
    "Herbalist.PcgSampleCell.HasAnInputPinUnlikeTheSourceNode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_HasAnInputPinUnlikeTheSourceNode::RunTest(const FString& Parameters)
{
    UPCGHerbalistSampleCellSettings* Sampler = NewObject<UPCGHerbalistSampleCellSettings>();
    if (!TestNotNull(TEXT("Sampler settings created"), Sampler)) return false;

    // Считать пины скопом нельзя: AllInputPinProperties() возвращает ещё и
    // авто-пины переопределения параметров (у «Get Herbalist Grid» их пять,
    // на чём этот тест сначала и упал). Значение имеет ровно один пин --
    // тот, куда приходят ДАННЫЕ, с меткой DefaultInputLabel.
    auto HasDataInputPin = [](const TArray<FPCGPinProperties>& Pins)
    {
        for (const FPCGPinProperties& Pin : Pins)
        {
            if (Pin.Label == PCGPinConstants::DefaultInputLabel) return true;
        }
        return false;
    };

    // Ровно то различие, ради которого узел написан. Проверяется не «функция
    // вернула массив», а следствие: у сэмплера вход для данных есть, у
    // источника его нет.
    TestTrue(TEXT("Sample Herbalist Cell declares a data input pin"),
        HasDataInputPin(Sampler->AllInputPinProperties()));
    TestTrue(TEXT("Sample Herbalist Cell declares an output pin"),
        Sampler->AllOutputPinProperties().Num() > 0);

    UPCGHerbalistGridSettings* Source = NewObject<UPCGHerbalistGridSettings>();
    if (!TestNotNull(TEXT("Source node settings created"), Source)) return false;

    TestFalse(TEXT("Get Herbalist Grid still has no data input pin -- it is a source, and that is why the sampler exists"),
        HasDataInputPin(Source->AllInputPinProperties()));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_DefaultsAreTheDocumentedOnes,
    "Herbalist.PcgSampleCell.DefaultsAreTheDocumentedOnes",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_DefaultsAreTheDocumentedOnes::RunTest(const FString& Parameters)
{
    UPCGHerbalistSampleCellSettings* Settings = NewObject<UPCGHerbalistSampleCellSettings>();
    if (!TestNotNull(TEXT("Node settings object created"), Settings)) return false;

    // Эти две строки пользователь вписывает в записи PCGMeshSelectorByAttribute
    // руками -- сопоставление идёт по строке, и смена дефолта здесь молча
    // рассогласовала бы уже настроенный спавнер.
    TestEqual(TEXT("Healthy key default"), Settings->HealthyMeshKey, FString(TEXT("Healthy")));
    TestEqual(TEXT("Degrading key default"), Settings->DegradingMeshKey, FString(TEXT("Degrading")));

    // Довод за этот дефолт -- в комментарии у самого свойства: тихо удалять
    // растительность за краем сетки значит принять решение с далеко идущими
    // последствиями для вида мира, и принимать его умолчанием неправильно.
    TestFalse(TEXT("Points outside the grid are kept by default -- dropping them is opt-in"),
        Settings->bDropPointsOutsideGrid);

    TestTrue(TEXT("Node is a PCG settings type the graph can instantiate"),
        Settings->IsA<UPCGSettings>());

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_AttributeNamesAreAStableContract,
    "Herbalist.PcgSampleCell.AttributeNamesAreAStableContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_AttributeNamesAreAStableContract::RunTest(const FString& Parameters)
{
    FString Source;
    if (!TestTrue(TEXT("Sampler source file is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistSampleCell.cpp"), Source)))
    {
        return false;
    }

    const TArray<FString> Expected = {
        TEXT("Distortion"),
        TEXT("Corruption"),
        TEXT("HarvestStress"),
        TEXT("Biome"),
        TEXT("bDegrading"),
        TEXT("MeshKey"),
    };

    for (const FString& Name : Expected)
    {
        const FString Needle = FString::Printf(TEXT("TEXT(\"%s\")"), *Name);
        TestTrue(FString::Printf(TEXT("Attribute '%s' is still declared by the sampler"), *Name),
            Source.Contains(Needle));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistPcgSampleCell_SharedAttributeNamesMatchTheSourceNode,
    "Herbalist.PcgSampleCell.SharedAttributeNamesMatchTheSourceNode",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistPcgSampleCell_SharedAttributeNamesMatchTheSourceNode::RunTest(const FString& Parameters)
{
    // Настоящая проверка на разъезд двух узлов. Оба кладут в точку одни и те
    // же оси мира; если один переименует Distortion, а другой нет, ни
    // компиляция, ни отдельные контрактные тесты этого не заметят -- каждый
    // останется верен себе. Ломается при этом граф пользователя.
    FString SamplerSource;
    FString GridSource;

    if (!TestTrue(TEXT("Sampler source is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistSampleCell.cpp"), SamplerSource)))
    {
        return false;
    }
    if (!TestTrue(TEXT("Grid source is readable"),
        LoadPcgSampleCellSource(TEXT("Source/ProjectHerbalist/Core/PCG/PCGHerbalistGridData.cpp"), GridSource)))
    {
        return false;
    }

    const TArray<FString> Shared = {
        TEXT("Distortion"),
        TEXT("Corruption"),
        TEXT("HarvestStress"),
        TEXT("Biome"),
    };

    for (const FString& Name : Shared)
    {
        const FString Needle = FString::Printf(TEXT("TEXT(\"%s\")"), *Name);
        TestTrue(FString::Printf(TEXT("Both nodes still spell '%s' the same way"), *Name),
            SamplerSource.Contains(Needle) && GridSource.Contains(Needle));
    }

    return true;
}

#endif // WITH_AUTOMATION_TESTS

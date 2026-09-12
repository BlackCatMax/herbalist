// Source/ProjectHerbalistTests/Private/Tests/TrampleFieldTest.cpp
//
// Поле вытоптанности (2026-09-12) -- чистая математика FTrampleField, без
// мира. Доводы за каждую формулу -- в шапке TrampleField.h.

#include "Misc/AutomationTest.h"
#include "Core/World/Trample/TrampleField.h"

#if WITH_AUTOMATION_TESTS

namespace
{
    // Фиксированный период распада для тестов поля: 1000 с на полное
    // зарастание, круглое число ради читаемых ожиданий.
    constexpr float TestClearSeconds = 1000.0f;

    float ClearFn(const FIntPoint&) { return TestClearSeconds; }

    // Центр текселя (I, J) чанка (0, 0) -- точки на оси штриха.
    FVector2D TexelCenter(int32 I, int32 J)
    {
        return FTrampleField::GetTexelCenter(FIntPoint(0, 0), I, J);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_ChordLengthMatchesGeometry,
    "Herbalist.Trample.Field.ChordLengthMatchesGeometry",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_ChordLengthMatchesGeometry::RunTest(const FString& Parameters)
{
    const FVector2D C(0.0, 0.0);
    const float R = 40.0f;

    TestEqual(TEXT("Отрезок через центр -- хорда 2R"),
        FTrampleField::ChordLengthInDisk(FVector2D(-100, 0), FVector2D(100, 0), C, R), 2.0f * R, 0.001f);
    TestEqual(TEXT("Смещение d -- хорда 2*sqrt(R^2 - d^2)"),
        FTrampleField::ChordLengthInDisk(FVector2D(-100, 20), FVector2D(100, 20), C, R),
        2.0f * FMath::Sqrt(R * R - 20.0f * 20.0f), 0.001f);
    TestEqual(TEXT("Отрезок, кончающийся в центре, -- половина"),
        FTrampleField::ChordLengthInDisk(FVector2D(-100, 0), FVector2D(0, 0), C, R), R, 0.001f);
    TestEqual(TEXT("Отрезок целиком внутри -- его длина"),
        FTrampleField::ChordLengthInDisk(FVector2D(-10, 0), FVector2D(10, 0), C, R), 20.0f, 0.001f);
    TestEqual(TEXT("Мимо круга -- ноль"),
        FTrampleField::ChordLengthInDisk(FVector2D(-100, 50), FVector2D(100, 50), C, R), 0.0f, 0.001f);
    TestEqual(TEXT("Нулевой отрезок -- ноль"),
        FTrampleField::ChordLengthInDisk(FVector2D(5, 5), FVector2D(5, 5), C, R), 0.0f, 0.001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_FullPassDepositsPassDepositOnCenterline,
    "Herbalist.Trample.Field.FullPassDepositsPassDepositOnCenterline",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_FullPassDepositsPassDepositOnCenterline::RunTest(const FString& Parameters)
{
    FTrampleField Field;
    const FVector2D Target = TexelCenter(60, 60);
    const float R = 40.0f;
    const float Deposit = 0.2f;

    // Полный проход вдоль X через центр текселя, с запасом больше радиуса.
    Field.AddStroke(Target - FVector2D(300, 0), Target + FVector2D(300, 0), R, Deposit, 0.0f, ClearFn);

    TestEqual(TEXT("На оси пути -- ровно PassDeposit"), Field.GetValueAt(Target, 0.0f, ClearFn), Deposit, 0.0001f);

    const FVector2D Side = TexelCenter(60, 61);   // на 25 см сбоку
    const float Expected = Deposit * FMath::Sqrt(1.0f - FMath::Square(25.0f / R));
    TestEqual(TEXT("Сбоку -- по профилю хорды sqrt(1 - (d/R)^2)"), Field.GetValueAt(Side, 0.0f, ClearFn), Expected, 0.0001f);

    TestEqual(TEXT("Дальше радиуса -- ноль"), Field.GetValueAt(TexelCenter(60, 63), 0.0f, ClearFn), 0.0f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_SegmentationDoesNotChangeTheField,
    "Herbalist.Trample.Field.SegmentationDoesNotChangeTheField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_SegmentationDoesNotChangeTheField::RunTest(const FString& Parameters)
{
    // Главное свойство штриха по хорде: путь, нарезанный на 17 кусков
    // неравной длины, даёт то же поле, что один отрезок. Иначе вытоптанность
    // зависела бы от частоты кадров.
    FTrampleField Whole;
    FTrampleField Pieces;
    const FVector2D From = TexelCenter(20, 30) + FVector2D(3.0, 7.0);
    const FVector2D To = TexelCenter(100, 70) + FVector2D(-5.0, 2.0);
    const float R = 42.0f;
    const float Deposit = 0.15f;

    Whole.AddStroke(From, To, R, Deposit, 0.0f, ClearFn);

    FVector2D Previous = From;
    float T = 0.0f;
    for (int32 Step = 1; Step <= 17; ++Step)
    {
        T = FMath::Min(1.0f, T + (Step % 3 == 0 ? 0.09f : 0.045f));
        if (Step == 17) T = 1.0f;
        const FVector2D Next = FMath::Lerp(From, To, T);
        Pieces.AddStroke(Previous, Next, R, Deposit, 0.0f, ClearFn);
        Previous = Next;
    }

    const FTrampleField::FChunk* A = Whole.FindChunk(FIntPoint(0, 0));
    const FTrampleField::FChunk* B = Pieces.FindChunk(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Целый штрих завёл чанк"), A) || !TestNotNull(TEXT("Нарезка завела чанк"), B)) return false;

    float MaxDifference = 0.0f;
    for (int32 Index = 0; Index < A->Values.Num(); ++Index)
    {
        MaxDifference = FMath::Max(MaxDifference, FMath::Abs(A->Values[Index] - B->Values[Index]));
    }
    TestTrue(FString::Printf(TEXT("Нарезка не меняет поле (макс. расхождение %.6f)"), MaxDifference), MaxDifference < 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_DecayIsLinearAndClearsInFullClearTime,
    "Herbalist.Trample.Field.DecayIsLinearAndClearsInFullClearTime",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_DecayIsLinearAndClearsInFullClearTime::RunTest(const FString& Parameters)
{
    FTrampleField Field;
    const FVector2D Target = TexelCenter(10, 10);

    // Насыщаем точку до 1.0 несколькими проходами.
    for (int32 Pass = 0; Pass < 10; ++Pass)
    {
        Field.AddStroke(Target - FVector2D(200, 0), Target + FVector2D(200, 0), 40.0f, 0.2f, 0.0f, ClearFn);
    }
    TestEqual(TEXT("Кламп в 1.0"), Field.GetValueAt(Target, 0.0f, ClearFn), 1.0f, 0.0001f);

    TestEqual(TEXT("Через четверть периода -- 0.75"), Field.GetValueAt(Target, 250.0f, ClearFn), 0.75f, 0.0001f);
    TestEqual(TEXT("Через половину -- 0.5"), Field.GetValueAt(Target, 500.0f, ClearFn), 0.5f, 0.0001f);
    TestEqual(TEXT("Через полный период -- ноль"), Field.GetValueAt(Target, 1000.0f, ClearFn), 0.0f, 0.0001f);

    const TArray<FIntPoint> Removed = Field.AdvanceAll(1000.0f, ClearFn);
    TestEqual(TEXT("Опустевший чанк удалён"), Removed.Num(), 1);
    TestNull(TEXT("Поля больше нет"), Field.FindChunk(FIntPoint(0, 0)));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_LazyAdvanceEqualsManySteps,
    "Herbalist.Trample.Field.LazyAdvanceEqualsManySteps",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_LazyAdvanceEqualsManySteps::RunTest(const FString& Parameters)
{
    // Ленивый догон точен: чанк, не тронутый 700 с, обязан совпасть с
    // чанком, которому распад применяли каждые 7 с.
    FTrampleField Lazy;
    FTrampleField Stepped;
    const FVector2D From = TexelCenter(0, 50);
    const FVector2D To = TexelCenter(127, 50);
    for (FTrampleField* Field : { &Lazy, &Stepped })
    {
        Field->AddStroke(From, To, 40.0f, 0.3f, 0.0f, ClearFn);
        Field->AddStroke(From, To, 40.0f, 0.3f, 0.0f, ClearFn);
    }

    for (int32 Step = 1; Step <= 100; ++Step)
    {
        Stepped.AdvanceChunk(FIntPoint(0, 0), Step * 7.0f, TestClearSeconds);
    }
    Lazy.AdvanceChunk(FIntPoint(0, 0), 700.0f, TestClearSeconds);

    const FTrampleField::FChunk* A = Lazy.FindChunk(FIntPoint(0, 0));
    const FTrampleField::FChunk* B = Stepped.FindChunk(FIntPoint(0, 0));
    if (!TestNotNull(TEXT("Lazy"), A) || !TestNotNull(TEXT("Stepped"), B)) return false;

    float MaxDifference = 0.0f;
    for (int32 Index = 0; Index < A->Values.Num(); ++Index)
    {
        MaxDifference = FMath::Max(MaxDifference, FMath::Abs(A->Values[Index] - B->Values[Index]));
    }
    TestTrue(FString::Printf(TEXT("Один шаг = сто шагов (макс. расхождение %.6f)"), MaxDifference), MaxDifference < 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_StrokeAcrossChunkBorderIsSeamless,
    "Herbalist.Trample.Field.StrokeAcrossChunkBorderIsSeamless",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_StrokeAcrossChunkBorderIsSeamless::RunTest(const FString& Parameters)
{
    // Путь вдоль X поперёк границы чанков (0,0) | (1,0): последние тексели
    // первого и первые второго на оси получают одно и то же.
    FTrampleField Field;
    const float Y = FTrampleField::GetTexelCenter(FIntPoint(0, 0), 0, 40).Y;
    const float Border = FTrampleField::ChunkSizeCm;
    Field.AddStroke(FVector2D(Border - 300.0f, Y), FVector2D(Border + 300.0f, Y), 40.0f, 0.2f, 0.0f, ClearFn);

    TestNotNull(TEXT("Левый чанк заведён"), Field.FindChunk(FIntPoint(0, 0)));
    TestNotNull(TEXT("Правый чанк заведён"), Field.FindChunk(FIntPoint(1, 0)));

    const float Left = Field.GetValueAt(FVector2D(Border - 12.5f, Y), 0.0f, ClearFn);
    const float Right = Field.GetValueAt(FVector2D(Border + 12.5f, Y), 0.0f, ClearFn);
    TestEqual(TEXT("Тексели по обе стороны границы -- одинаковы"), Left, Right, 0.0001f);
    TestEqual(TEXT("И равны полному проходу"), Left, 0.2f, 0.0001f);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_SampleTexelRectMatchesPointQueries,
    "Herbalist.Trample.Field.SampleTexelRectMatchesPointQueries",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_SampleTexelRectMatchesPointQueries::RunTest(const FString& Parameters)
{
    // Выборка прямоугольника -- то, чем окно показа читает поле. Обязана
    // совпадать с точечным запросом в каждом текселе, в том числе через
    // отрицательные координаты и стык четырёх чанков, и с учётом распада.
    FTrampleField Field;
    Field.AddStroke(FVector2D(-900.0, -700.0), FVector2D(800.0, 650.0), 45.0f, 0.4f, 0.0f, ClearFn);
    Field.AddStroke(FVector2D(-600.0, 500.0), FVector2D(700.0, -400.0), 35.0f, 0.3f, 0.0f, ClearFn);

    const float Now = 123.0f;
    const int32 MinGX = -50;
    const int32 MinGY = -40;
    const int32 Width = 97;
    const int32 Height = 83;
    TArray<float> Values;
    Field.SampleTexelRect(MinGX, MinGY, Width, Height, Now, ClearFn, Values);
    if (!TestEqual(TEXT("Размер выборки"), Values.Num(), Width * Height)) return false;

    int32 Mismatches = 0;
    int32 NonZero = 0;
    for (int32 Row = 0; Row < Height; ++Row)
    {
        for (int32 Col = 0; Col < Width; ++Col)
        {
            const FVector2D Center((MinGX + Col + 0.5) * FTrampleField::TexelSizeCm, (MinGY + Row + 0.5) * FTrampleField::TexelSizeCm);
            const float Expected = Field.GetValueAt(Center, Now, ClearFn);
            const float Actual = Values[Row * Width + Col];
            if (!FMath::IsNearlyEqual(Expected, Actual, 0.00001f)) ++Mismatches;
            if (Actual > 0.0f) ++NonZero;
        }
    }
    TestEqual(TEXT("Каждый тексель совпадает с точечным запросом"), Mismatches, 0);
    TestTrue(TEXT("Выборка задела натоптанное"), NonZero > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FHerbalistTrampleField_SaveQuantizationRoundTrips,
    "Herbalist.Trample.Field.SaveQuantizationRoundTrips",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FHerbalistTrampleField_SaveQuantizationRoundTrips::RunTest(const FString& Parameters)
{
    const TArray<float> Original = { 0.0f, 1.0f, 0.5f, 1.0f / 7.0f, 0.123456f, 0.999999f };
    TArray<uint16> Quantized;
    TArray<float> Restored;
    FTrampleField::QuantizeValues(Original, Quantized);
    FTrampleField::DequantizeValues(Quantized, Restored);

    if (!TestEqual(TEXT("Длина сохранена"), Restored.Num(), Original.Num())) return false;
    for (int32 Index = 0; Index < Original.Num(); ++Index)
    {
        TestEqual(FString::Printf(TEXT("Значение %d в пределах шага 1/65535"), Index), Restored[Index], Original[Index], 1.0f / 65535.0f);
    }

    FTrampleField Field;
    TArray<float> WrongSize;
    WrongSize.SetNumZeroed(10);
    TestFalse(TEXT("Чанк чужого размера не принимается"), Field.SetChunkValues(FIntPoint(0, 0), MoveTemp(WrongSize), 0.0f));
    return true;
}

#endif // WITH_AUTOMATION_TESTS

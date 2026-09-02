// ArtifactIngredientAppendCommandlet.cpp
#include "Commandlets/ArtifactIngredientAppendCommandlet.h"
#include "Core/Data/IngredientTableRow.h"
#include "Engine/DataTable.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"

namespace
{
    struct FArtifactRowSeed
    {
        FName ID;
        const TCHAR* DisplayName;
        const TCHAR* Description;
    };

    // Тексты — короткая, не сюжетная выжимка уже написанного в
    // 21_Journey_And_Artifacts.md §21.3/16_Entity_Manifestation.md §16.4,
    // не новый лор: то, что игрок увидел бы в тултипе, не полная карточка.
    const FArtifactRowSeed ArtifactRows[] = {
        { FName(TEXT("Зеркальце")),           TEXT("Зеркальце"),           TEXT("Наблюдение, не перемещение — показывает Заряну из любой основанной базы.") },
        { FName(TEXT("Клубочек")),            TEXT("Клубочек"),            TEXT("Ведёт между уже основанными базами. Не мгновенно — дорога отнимает время.") },
        { FName(TEXT("Рог")),                 TEXT("Рог"),                 TEXT("Слушает воду. Поднесённый к роднику или колодцу, отвечает, чист источник или испорчен.") },
        { FName(TEXT("Гребень")),             TEXT("Гребень"),             TEXT("Брошенный, мгновенно выращивает заросль позади — способ уйти из опасного места.") },
        { FName(TEXT("Молодильное яблоко")),  TEXT("Молодильное яблоко"),  TEXT("Съеденное рядом с Заряной, на время убирает шум с её росы.") },
        { FName(TEXT("Шапка-невидимка")),     TEXT("Шапка-невидимка"),     TEXT("Пока надета, мир на время не замечает, что здесь кто-то есть.") },
        { FName(TEXT("Камень-оберег")),       TEXT("Камень-оберег"),       TEXT("Держишь его во время варки — гасит худший исход один раз, не гарантирует успех.") },
        { FName(TEXT("Фонарь")),              TEXT("Фонарь"),              TEXT("Негаснущий свет. Добыт ложью — цена за это не мгновенная, а собственная.") },
        { FName(TEXT("Перо Гамаюна")),        TEXT("Перо Гамаюна"),        TEXT("Перо вещей птицы истины. Съеденное, делает пророческое зрение Зеркальца постоянным.") },
        { FName(TEXT("Перо Алконоста")),      TEXT("Перо Алконоста"),      TEXT("Перо птицы радости. Её песня на время не подпускает морок ко всему краю.") },
        { FName(TEXT("Перо Сирина")),         TEXT("Перо Сирина"),         TEXT("Перо птицы тоски. Позволяет один раз честно увидеть правду сквозь худший морок без вреда.") },
        { FName(TEXT("Перо Жар-птицы")),      TEXT("Перо Жар-птицы"),      TEXT("Неувядающее перо. Отмеченная им земля больше никогда не деградирует.") },
    };
}

int32 UArtifactIngredientAppendCommandlet::Main(const FString& Params)
{
    const TCHAR* AssetPath = TEXT("/Game/Herbalist/Data/DT_IngredientClass");
    UDataTable* Table = LoadObject<UDataTable>(nullptr, AssetPath);
    if (!Table)
    {
        UE_LOG(LogTemp, Error, TEXT("ArtifactIngredientAppend: не удалось загрузить %s"), AssetPath);
        return 1;
    }

    int32 AddedCount = 0;
    int32 SkippedCount = 0;
    for (const FArtifactRowSeed& Seed : ArtifactRows)
    {
        if (Table->GetRowMap().Contains(Seed.ID))
        {
            UE_LOG(LogTemp, Warning, TEXT("ArtifactIngredientAppend: ряд '%s' уже существует, пропущен"), *Seed.ID.ToString());
            ++SkippedCount;
            continue;
        }

        FIngredientTableRow Row;
        Row.DisplayName = FText::FromString(Seed.DisplayName);
        Row.Description = FText::FromString(Seed.Description);
        Row.Class = EIngredientClass::Essence;   // ближайший честный эквивалент -- не растение/минерал/гриб
        Row.bIsWater = false;
        // AllowedBiomes намеренно пуст -- никогда не выпадает случайным
        // сбором, только через собственный Exec-путь получения.
        Row.RarityWeight = 1;
        Row.DecayRate = 0.0f;   // артефакты не портятся
        Row.Resilience = 1.0f;  // не поддаётся месту вовсе -- не то, что вообще может быть "собрано" из клетки

        Table->AddRow(Seed.ID, Row);
        ++AddedCount;
    }

    if (AddedCount == 0)
    {
        UE_LOG(LogTemp, Display, TEXT("ArtifactIngredientAppend: нечего добавлять (%d уже существует), пакет не сохранён"), SkippedCount);
        return 0;
    }

    Table->MarkPackageDirty();

    UPackage* Package = Table->GetOutermost();
    const FString PackageFileName = FPackageName::LongPackageNameToFilename(
        Package->GetName(), FPackageName::GetAssetPackageExtension());

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    SaveArgs.SaveFlags = SAVE_NoError;

    const bool bSuccess = UPackage::SavePackage(Package, Table, *PackageFileName, SaveArgs);
    if (!bSuccess)
    {
        UE_LOG(LogTemp, Error, TEXT("ArtifactIngredientAppend: не удалось сохранить пакет %s"), *PackageFileName);
        return 1;
    }

    UE_LOG(LogTemp, Display, TEXT("ArtifactIngredientAppend: %s теперь содержит %d рядов (добавлено %d, пропущено %d)"),
        AssetPath, Table->GetRowMap().Num(), AddedCount, SkippedCount);
    return 0;
}

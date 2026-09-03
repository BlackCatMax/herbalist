// CompendiumAuditCommandlet.cpp
#include "Commandlets/CompendiumAuditCommandlet.h"

#include "Core/Data/IngredientTableRow.h"
#include "Core/Entities/AmbientEntityTypes.h"
#include "Core/Entities/LandmarkTypes.h"
#include "Core/Entities/LegendaryEntityTypes.h"
#include "Core/Types/BiomeTypes.h"

#include "Engine/DataTable.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

namespace
{
    // ---- Минимальный разбор YAML-фронтматтера ----
    // Полноценный YAML не нужен и был бы лишней зависимостью: карточки
    // используют ровно три формы -- скаляр, список [a, b] и вики-ссылку
    // [[Имя]]. Проверено на всех 154 карточках: у растений фронтматтер
    // единообразен до последнего ключа (76 из 76), у бестиария различается
    // только набором необязательных полей.
    FString StripWikiBrackets(const FString& In)
    {
        FString S = In;
        S.TrimStartAndEndInline();
        S.RemoveFromStart(TEXT("[["));
        S.RemoveFromEnd(TEXT("]]"));
        S.TrimStartAndEndInline();
        S.RemoveFromStart(TEXT("\""));
        S.RemoveFromEnd(TEXT("\""));
        return S.TrimStartAndEnd();
    }

    TMap<FString, FString> ParseFrontmatter(const FString& FileContent)
    {
        TMap<FString, FString> Out;

        TArray<FString> Lines;
        FileContent.ParseIntoArrayLines(Lines, /*bCullEmpty=*/false);

        int32 Index = 0;
        while (Index < Lines.Num() && Lines[Index].TrimStartAndEnd().IsEmpty()) ++Index;
        if (Index >= Lines.Num() || Lines[Index].TrimStartAndEnd() != TEXT("---")) return Out;

        for (++Index; Index < Lines.Num(); ++Index)
        {
            const FString Line = Lines[Index];
            if (Line.TrimStartAndEnd() == TEXT("---")) break;

            int32 Colon = INDEX_NONE;
            if (!Line.FindChar(TEXT(':'), Colon)) continue;

            const FString Key = Line.Left(Colon).TrimStartAndEnd();
            FString Value = Line.Mid(Colon + 1).TrimStartAndEnd();
            if (Key.IsEmpty()) continue;

            // Блочная форма списка -- значение не на той же строке, а
            // отдельными пунктами ниже:
            //     d_base:
            //       - 0.40
            //       - 0.80
            // Без этой ветки у 17 карточек d_base и у 12 biome разбирались
            // пустыми, и аудит сообщал о несуществующих проблемах (поймано
            // первым же прогоном на реальном компендиуме).
            if (Value.IsEmpty())
            {
                TArray<FString> Items;
                int32 Look = Index + 1;
                for (; Look < Lines.Num(); ++Look)
                {
                    const FString Next = Lines[Look].TrimStartAndEnd();
                    if (Next == TEXT("---")) break;
                    if (!Next.StartsWith(TEXT("- "))) break;

                    // Дефис может быть не один: вики-ссылка [[Болото]],
                    // развёрнутая в блочный YAML, даёт вложенный список
                    // "  - - Болото". Снимаем все уровни.
                    FString Item = Next;
                    while (Item.StartsWith(TEXT("- ")))
                    {
                        Item = Item.Mid(2).TrimStartAndEnd();
                    }
                    Items.Add(Item);
                }
                if (Items.Num() > 0)
                {
                    Value = FString::Printf(TEXT("[%s]"), *FString::Join(Items, TEXT(", ")));
                    Index = Look - 1;
                }
            }

            Out.Add(Key, Value);
        }
        return Out;
    }

    // "[0.40, 0.80, 0.60, 0.20]" -> четыре числа. Порядок осей в карточках --
    // Body, Mind, Spirit, Nature (подтверждено радар-диаграммой в самих
    // карточках, где подписи идут именно так).
    bool ParseFloatList(const FString& In, TArray<float>& Out)
    {
        FString S = In;
        S.TrimStartAndEndInline();
        S.RemoveFromStart(TEXT("["));
        S.RemoveFromEnd(TEXT("]"));

        TArray<FString> Parts;
        S.ParseIntoArray(Parts, TEXT(","), /*InCullEmpty=*/true);
        Out.Reset();
        for (FString& Part : Parts)
        {
            Part.TrimStartAndEndInline();
            if (!Part.IsNumeric()) return false;
            Out.Add(FCString::Atof(*Part));
        }
        return Out.Num() > 0;
    }

    bool ParseFloat(const TMap<FString, FString>& FM, const TCHAR* Key, float& Out)
    {
        if (const FString* Found = FM.Find(Key))
        {
            const FString Trimmed = Found->TrimStartAndEnd();
            if (Trimmed.IsNumeric()) { Out = FCString::Atof(*Trimmed); return true; }
        }
        return false;
    }

    // Карточка может называться тремя способами сразу: файлом
    // ("Мухомор красный.md"), полем name ("Мухомор (Мушиный гриб, Лесной
    // шут)") и его первой частью до скобки. Какой из них лёг в ключ строки
    // таблицы -- заранее неизвестно, поэтому пробуем все и потом сообщаем,
    // чем именно совпало: расхождение в способе именования само по себе
    // находка.
    TArray<FString> BuildCandidateKeys(const FString& FilePath, const TMap<FString, FString>& FM)
    {
        TArray<FString> Keys;

        // Поле id -- первым: именно оно оказалось ключом строк
        // DT_IngredientClass ('bol_01', 'mix_09'), то есть таблица когда-то
        // и была сгенерирована из этих карточек. Выяснено самим этим
        // аудитом: без id не совпала НИ ОДНА из 76 карточек.
        if (const FString* Id = FM.Find(TEXT("id")))
        {
            const FString Trimmed = StripWikiBrackets(*Id);
            if (!Trimmed.IsEmpty()) Keys.Add(Trimmed);
        }

        Keys.AddUnique(FPaths::GetBaseFilename(FilePath));

        if (const FString* Name = FM.Find(TEXT("name")))
        {
            const FString Full = StripWikiBrackets(*Name);
            Keys.AddUnique(Full);

            int32 Paren = INDEX_NONE;
            if (Full.FindChar(TEXT('('), Paren) && Paren > 0)
            {
                Keys.AddUnique(Full.Left(Paren).TrimStartAndEnd());
            }
        }
        return Keys;
    }

    struct FCard
    {
        FString FilePath;
        FString Section;        // Растительность / Бестиарий
        FString Level;          // ранг для бестиария
        TMap<FString, FString> Frontmatter;
        TArray<FString> Keys;
    };

    void CollectCards(const FString& Root, const FString& Section, TArray<FCard>& OutCards)
    {
        TArray<FString> Files;
        IFileManager::Get().FindFilesRecursive(Files, *(Root / Section), TEXT("*.md"), true, false);

        for (const FString& File : Files)
        {
            FString Content;
            if (!FFileHelper::LoadFileToString(Content, *File)) continue;

            FCard Card;
            Card.FilePath = File;
            Card.Section = Section;
            Card.Frontmatter = ParseFrontmatter(Content);
            if (Card.Frontmatter.Num() == 0) continue;   // не карточка, а обычная заметка

            if (const FString* Level = Card.Frontmatter.Find(TEXT("level")))
            {
                Card.Level = StripWikiBrackets(*Level);
            }
            Card.Keys = BuildCandidateKeys(File, Card.Frontmatter);
            OutCards.Add(MoveTemp(Card));
        }
    }

    // Тот же словарь, что уже у extract_biomes.py в корне репозитория --
    // компендиум называет биомы по-русски, код держит enum.
    const TMap<FString, EBiomeType>& RussianBiomeMap()
    {
        static const TMap<FString, EBiomeType> Map = {
            { TEXT("Болото"),                 EBiomeType::Bog },
            { TEXT("Лесостепь"),              EBiomeType::ForestSteppe },
            { TEXT("Речная пойма"),           EBiomeType::Floodplain },
            { TEXT("Смешанный лес"),          EBiomeType::MixedForest },
            { TEXT("Широколиственный лес"),   EBiomeType::BroadleafForest },
            { TEXT("Степь"),                  EBiomeType::Steppe },
            { TEXT("Тайга"),                  EBiomeType::Taiga },
            { TEXT("Тундра"),                 EBiomeType::Tundra },
        };
        return Map;
    }

    bool NearlyEqual(float A, float B) { return FMath::Abs(A - B) <= 0.006f; }

    void ReportMismatch(const FString& CardName, const TCHAR* Field, float CardValue, float RowValue, int32& Counter)
    {
        UE_LOG(LogTemp, Warning, TEXT("    %s: карточка %.2f, таблица %.2f  [%s]"), Field, CardValue, RowValue, *CardName);
        ++Counter;
    }
}

int32 UCompendiumAuditCommandlet::Main(const FString& Params)
{
    // Компендиум лежит рядом с проектом, не внутри него.
    FString CompendiumPath;
    if (!FParse::Value(*Params, TEXT("CompendiumPath="), CompendiumPath))
    {
        CompendiumPath = FPaths::Combine(FPaths::ProjectDir(), TEXT("../herbalist_docs/Herbalist_Vault/04_Compendium"));
    }
    FPaths::CollapseRelativeDirectories(CompendiumPath);

    if (!IFileManager::Get().DirectoryExists(*CompendiumPath))
    {
        UE_LOG(LogTemp, Error, TEXT("CompendiumAudit: не найден компендиум по пути %s"), *CompendiumPath);
        return 1;
    }
    UE_LOG(LogTemp, Display, TEXT("CompendiumAudit: читаю %s"), *CompendiumPath);

    TArray<FCard> Plants, Beasts;
    CollectCards(CompendiumPath, TEXT("Растительность"), Plants);
    CollectCards(CompendiumPath, TEXT("Бестиарий"), Beasts);
    UE_LOG(LogTemp, Display, TEXT("CompendiumAudit: карточек -- растений %d, бестиария %d"), Plants.Num(), Beasts.Num());

    int32 Problems = 0;

    // ---------------- Растения -> DT_IngredientClass ----------------
    UDataTable* IngredientTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_IngredientClass"));
    if (!IngredientTable)
    {
        UE_LOG(LogTemp, Error, TEXT("CompendiumAudit: DT_IngredientClass не загрузился"));
        return 1;
    }

    TSet<FName> MatchedRows;
    UE_LOG(LogTemp, Display, TEXT("=== РАСТЕНИЯ (%d карточек против %d строк) ==="), Plants.Num(), IngredientTable->GetRowMap().Num());

    for (const FCard& Card : Plants)
    {
        FIngredientTableRow* Row = nullptr;
        FName MatchedBy = NAME_None;
        for (const FString& Key : Card.Keys)
        {
            Row = IngredientTable->FindRow<FIngredientTableRow>(FName(*Key), TEXT("CompendiumAudit"), /*bWarnIfMissing=*/false);
            if (Row) { MatchedBy = FName(*Key); break; }
        }

        if (!Row)
        {
            UE_LOG(LogTemp, Warning, TEXT("  НЕТ СТРОКИ: %s (пробовал: %s)"),
                *FPaths::GetBaseFilename(Card.FilePath), *FString::Join(Card.Keys, TEXT(" | ")));
            ++Problems;
            continue;
        }
        MatchedRows.Add(MatchedBy);

        const FString CardName = FPaths::GetBaseFilename(Card.FilePath);

        // Мета-оси
        float Value = 0.0f;
        if (ParseFloat(Card.Frontmatter, TEXT("purity"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Purity))
            ReportMismatch(CardName, TEXT("purity"), Value, Row->BaseState.Meta.Purity, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("corruption"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Corruption))
            ReportMismatch(CardName, TEXT("corruption"), Value, Row->BaseState.Meta.Corruption, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("distortion"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Distortion))
            ReportMismatch(CardName, TEXT("distortion"), Value, Row->BaseState.Meta.Distortion, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("stability"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Stability))
            ReportMismatch(CardName, TEXT("stability"), Value, Row->BaseState.Meta.Stability, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("potency"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Potency))
            ReportMismatch(CardName, TEXT("potency"), Value, Row->BaseState.Meta.Potency, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("resonance"), Value) && !NearlyEqual(Value, Row->BaseState.Meta.Resonance))
            ReportMismatch(CardName, TEXT("resonance"), Value, Row->BaseState.Meta.Resonance, Problems);
        if (ParseFloat(Card.Frontmatter, TEXT("m_base"), Value) && !NearlyEqual(Value, Row->BaseState.Magnitude))
            ReportMismatch(CardName, TEXT("m_base"), Value, Row->BaseState.Magnitude, Problems);

        // Оси направления: d_base = [Body, Mind, Spirit, Nature]
        if (const FString* DBase = Card.Frontmatter.Find(TEXT("d_base")))
        {
            TArray<float> Axes;
            if (ParseFloatList(*DBase, Axes) && Axes.Num() == 4)
            {
                if (!NearlyEqual(Axes[0], Row->BaseState.Direction.Body))
                    ReportMismatch(CardName, TEXT("d_base.Body"), Axes[0], Row->BaseState.Direction.Body, Problems);
                if (!NearlyEqual(Axes[1], Row->BaseState.Direction.Mind))
                    ReportMismatch(CardName, TEXT("d_base.Mind"), Axes[1], Row->BaseState.Direction.Mind, Problems);
                if (!NearlyEqual(Axes[2], Row->BaseState.Direction.Spirit))
                    ReportMismatch(CardName, TEXT("d_base.Spirit"), Axes[2], Row->BaseState.Direction.Spirit, Problems);
                if (!NearlyEqual(Axes[3], Row->BaseState.Direction.Nature))
                    ReportMismatch(CardName, TEXT("d_base.Nature"), Axes[3], Row->BaseState.Direction.Nature, Problems);
            }
            else
            {
                UE_LOG(LogTemp, Warning, TEXT("  НЕ РАЗОБРАН d_base у %s: %s"), *CardName, **DBase);
                ++Problems;
            }
        }

        // Биом: карточка называет его по-русски, таблица хранит enum.
        if (const FString* BiomeStr = Card.Frontmatter.Find(TEXT("biome")))
        {
            // biome может быть и скаляром "[[Болото]]", и списком
            // "[[[Болото]], [[Тайга]]]" -- берём первый элемент, остальные
            // проверять нечем: карточка не говорит, какой из них главный.
            FString First = *BiomeStr;
            First.TrimStartAndEndInline();
            {
                // Список -- берём первый элемент: карточка не говорит, какой
                // из нескольких биомов главный, а таблица хранит их набором.
                int32 Comma = INDEX_NONE;
                if (First.FindChar(TEXT(','), Comma)) First = First.Left(Comma);
                // Скобок может быть сколько угодно уровней: и от вики-ссылки
                // [[Болото]], и от нашей же сборки блочного списка в "[...]".
                while (First.StartsWith(TEXT("["))) First.RemoveFromStart(TEXT("["));
                while (First.EndsWith(TEXT("]"))) First.RemoveFromEnd(TEXT("]"));
            }
            const FString CardBiome = StripWikiBrackets(First);
            const EBiomeType* Mapped = RussianBiomeMap().Find(CardBiome);

            if (!Mapped)
            {
                UE_LOG(LogTemp, Warning, TEXT("  НЕИЗВЕСТНЫЙ биом '%s' у %s"), *CardBiome, *CardName);
                ++Problems;
            }
            else if (Row->AllowedBiomes.Num() == 0)
            {
                // Пустой список у ингредиента значит «не выпадает случайным
                // сбором» (так заведены артефакты и перья) -- для растения из
                // компендиума это почти наверняка недосмотр.
                UE_LOG(LogTemp, Warning, TEXT("  ПУСТОЙ AllowedBiomes у %s (карточка: %s)"), *CardName, *CardBiome);
                ++Problems;
            }
            else if (!Row->AllowedBiomes.Contains(*Mapped))
            {
                UE_LOG(LogTemp, Warning, TEXT("  БИОМ РАСХОДИТСЯ у %s: карточка '%s', в таблице его нет среди %d разрешённых"),
                    *CardName, *CardBiome, Row->AllowedBiomes.Num());
                ++Problems;
            }
        }
    }

    // Строки без карточки
    for (const TPair<FName, uint8*>& Pair : IngredientTable->GetRowMap())
    {
        if (!MatchedRows.Contains(Pair.Key))
        {
            UE_LOG(LogTemp, Warning, TEXT("  НЕТ КАРТОЧКИ: строка '%s'"), *Pair.Key.ToString());
            ++Problems;
        }
    }

    // ---------------- Бестиарий -> три таблицы рангов ----------------
    UDataTable* AmbientTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_AmbientEntities"));
    UDataTable* LandmarkTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_Landmarks"));
    UDataTable* LegendaryTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_LegendaryEntities"));

    int32 CountLow = 0, CountMain = 0, CountLegend = 0, CountNightmare = 0, CountUnknownLevel = 0;
    int32 MissingLow = 0, MissingMain = 0, MissingLegend = 0;

    UE_LOG(LogTemp, Display, TEXT("=== БЕСТИАРИЙ (%d карточек) ==="), Beasts.Num());

    for (const FCard& Card : Beasts)
    {
        UDataTable* Target = nullptr;
        int32* MissingCounter = nullptr;

        if (Card.Level == TEXT("Низший")) { Target = AmbientTable; ++CountLow; MissingCounter = &MissingLow; }
        else if (Card.Level == TEXT("Основной")) { Target = LandmarkTable; ++CountMain; MissingCounter = &MissingMain; }
        else if (Card.Level == TEXT("Легендарный")) { Target = LegendaryTable; ++CountLegend; MissingCounter = &MissingLegend; }
        else if (Card.Level.Contains(TEXT("нечисть"))) { ++CountNightmare; continue; }   // §16.5, реализована глобальным ночным нуджем, строк не имеет по дизайну
        else { ++CountUnknownLevel;
            UE_LOG(LogTemp, Warning, TEXT("  НЕИЗВЕСТНЫЙ ранг '%s' у %s"), *Card.Level, *FPaths::GetBaseFilename(Card.FilePath));
            ++Problems; continue; }

        if (!Target) continue;

        bool bFound = false;
        for (const FString& Key : Card.Keys)
        {
            if (Target->GetRowMap().Contains(FName(*Key))) { bFound = true; break; }
        }
        if (!bFound)
        {
            UE_LOG(LogTemp, Warning, TEXT("  НЕТ СТРОКИ [%s]: %s"), *Card.Level, *FPaths::GetBaseFilename(Card.FilePath));
            if (MissingCounter) ++(*MissingCounter);
            ++Problems;
        }
    }

    UE_LOG(LogTemp, Display, TEXT("--- ИТОГ ---"));
    UE_LOG(LogTemp, Display, TEXT("Растения: %d карточек, %d строк в таблице"), Plants.Num(), IngredientTable->GetRowMap().Num());
    UE_LOG(LogTemp, Display, TEXT("Низший:       карточек %d, строк %d, без строки %d"),
        CountLow, AmbientTable ? AmbientTable->GetRowMap().Num() : -1, MissingLow);
    UE_LOG(LogTemp, Display, TEXT("Основной:     карточек %d, строк %d, без строки %d"),
        CountMain, LandmarkTable ? LandmarkTable->GetRowMap().Num() : -1, MissingMain);
    UE_LOG(LogTemp, Display, TEXT("Легендарный:  карточек %d, строк %d, без строки %d"),
        CountLegend, LegendaryTable ? LegendaryTable->GetRowMap().Num() : -1, MissingLegend);
    UE_LOG(LogTemp, Display, TEXT("Опасная нечисть: %d карточек -- строк не имеют ПО ДИЗАЙНУ (§16.5, глобальный ночной нудж)"), CountNightmare);
    UE_LOG(LogTemp, Display, TEXT("Всего расхождений: %d"), Problems);
    UE_LOG(LogTemp, Display, TEXT("Ничего не изменено -- это только сверка."));

    return 0;
}

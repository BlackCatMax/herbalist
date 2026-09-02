// MemoryFragmentDefinitions.h
#pragma once

#include "CoreMinimal.h"
#include "Core/Zaryana/MemoryFragmentTypes.h"

// Тексты — не заимствованы, написаны в тон уже принятому в проекте
// фольклорному языку (см. компендиум, 17_Hero_And_Community.md — Аграфена
// как наставница). Ложные версии несут скрытый сбой — деталь, противоречащую
// уже установленному канону (кто где был, чей это был характер) — игрок
// должен заметить несостыковку сам, не через подсказку в UI.
//
// 2026-09-02, Unit 6/6 миграции контента проекта на DataTable — тот же
// паттерн, что весь бестиарий/Artifact/Dialogue: GetAllMemoryFragmentDefinitions()
// ниже лениво грузит /Game/Herbalist/Data/DT_MemoryFragments (см. подробное
// обоснование в AmbientEntityTypes.h). LogTemp, не HerbalistLogChannels.h
// категория — та же причина (LNK2001): inline-функция компилируется и в
// ProjectHerbalistTests через новый коммандлет.
namespace HerbalistCore::Zaryana
{
    inline const TArray<FMemoryFragmentDefinition>& GetAllMemoryFragmentDefinitions()
    {
        static const TArray<FMemoryFragmentDefinition> Definitions = []()
        {
            check(IsInGameThread());   // LoadObject не потокобезопасен

            TArray<FMemoryFragmentDefinition> Out;
            UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/Herbalist/Data/DT_MemoryFragments"));
            if (!Table)
            {
                UE_LOG(LogTemp, Error, TEXT("GetAllMemoryFragmentDefinitions: не удалось загрузить DT_MemoryFragments -- реестр фрагментов памяти будет пуст"));
                return Out;
            }
            Table->AddToRoot();

            TArray<FMemoryFragmentDefinition*> Rows;
            Table->GetAllRows(TEXT("GetAllMemoryFragmentDefinitions"), Rows);
            Out.Reserve(Rows.Num());
            for (const FMemoryFragmentDefinition* Row : Rows)
            {
                if (Row) Out.Add(*Row);
            }
            Out.Sort([](const FMemoryFragmentDefinition& A, const FMemoryFragmentDefinition& B) { return A.SortOrder < B.SortOrder; });
            return Out;
        }();
        return Definitions;
    }

    inline const FMemoryFragmentDefinition* FindMemoryFragmentDefinition(FName ID)
    {
        for (const FMemoryFragmentDefinition& Def : GetAllMemoryFragmentDefinitions())
        {
            if (Def.ID == ID) return &Def;
        }
        return nullptr;
    }
}

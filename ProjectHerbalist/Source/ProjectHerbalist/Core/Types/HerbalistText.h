// Core/Types/HerbalistText.h
//
// Заглавная буква для русского текста (аудит кода 2026-09-26, Б5).
// FChar::ToUpper и FString::ToUpper меняют только латиницу (движок: «Only
// converts ASCII characters») или зависят от локали C-рантайма -- на
// кириллице это тихий no-op. Здесь -- явный пересчёт по кодовым точкам:
// а..я -> А..Я, ё -> Ё, латиница -- как в движке.
#pragma once

#include "CoreMinimal.h"

namespace HerbalistCore::Text
{
    inline TCHAR ToUpperRu(TCHAR Char)
    {
        if (Char >= TEXT('а') && Char <= TEXT('я'))
        {
            return static_cast<TCHAR>(Char - (TEXT('а') - TEXT('А')));
        }
        if (Char == TEXT('ё'))
        {
            return TEXT('Ё');
        }
        return FChar::ToUpper(Char);
    }

    // Первая буква -- заглавная, остальное как есть.
    inline FString CapitalizeFirst(const FString& Text)
    {
        if (Text.IsEmpty())
        {
            return Text;
        }
        FString Result = Text;
        Result[0] = ToUpperRu(Result[0]);
        return Result;
    }
}

// ArtifactIngredientAppendCommandlet.h
//
// Добавляет ряды для восьми артефактов Легендарных (21_Journey_And_Artifacts.md
// §21.3) и четырёх перьев вещих птиц (16_Entity_Manifestation.md §16.4) в живой
// DT_IngredientClass (тот же ассет, что уже несёт обычные травы), не трогая
// уже существующие ряды — тот же UDataTable::AddRow-приём, что и
// IngredientAppendCommandlet.cpp, но строит FIngredientTableRow программно в
// C++, не из JSON: этих 12 строк нет ни в компендиуме, ни в
// herbalist_docs/CSV_tabs/ingredients.json — они не растения, не про сбор.
//
// AllowedBiomes пуст у всех 12 -- та же семантика, что уже устоялась для
// поля ("пустой список значит нигде не растёт"): гарантирует, что артефакт
// никогда не выпадет случайным сбором, только через свой собственный
// Exec-путь (TryAcquireArtifact/TryAcquireProphetFeather/...). DecayRate=0 --
// артефакты не портятся, как обычные травы (используется вместе с
// FInventoryItem::bSubjectToDecay=false на стороне вызывающего кода, оба
// защитных слоя, не один).
//
// Идемпотентен: ряд, уже присутствующий в таблице (найден по имени),
// пропускается с предупреждением, не перезаписывается.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=ArtifactIngredientAppend
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "ArtifactIngredientAppendCommandlet.generated.h"

UCLASS()
class UArtifactIngredientAppendCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};

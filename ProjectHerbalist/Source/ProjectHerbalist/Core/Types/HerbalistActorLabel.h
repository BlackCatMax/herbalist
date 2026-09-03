// Core/Types/HerbalistActorLabel.h
//
// Подпись актора для World Outliner и выделения в PIE (2026-09-03, прямая
// жалоба пользователя: "при выделении акторов в PIE мне не понятно, что
// это за актор, т.к. заглушка одна на всех. Аналогично с именами
// ресурсов").
//
// Пока у всего бестиария один меш-заглушка (PlaceholderEntityMesh) и у
// ресурсов меш из строки таблицы, имя в аутлайнере -- единственное, что
// отличает Гнильников от Лешего, а зверобой от крапивы. Движковое имя
// вида "BP_HerbalistResourceActor_C_37" не говорит ничего.
//
// Только редактор: SetActorLabel объявлен под WITH_EDITOR, в кукнутой
// сборке его нет вовсе -- и это правильно, метка чисто авторская, игрок
// её не видит никогда.
//
// bMarkDirty=false намеренно: подписываются в том числе PIE-акторы, и
// помечать из-за косметической метки пакет уровня грязным незачем.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

inline void SetHerbalistDebugLabel(AActor* Actor, const FString& Label)
{
#if WITH_EDITOR
    if (Actor && !Label.IsEmpty())
    {
        Actor->SetActorLabel(Label, /*bMarkDirty=*/false);
    }
#else
    // Не-редакторская сборка: параметры не используются, но и предупреждения
    // о них не нужны.
    (void)Actor;
    (void)Label;
#endif
}

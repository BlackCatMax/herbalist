// AmbientSpawnerTypes.h
//
// Спавнеры Низших (DESIGN_Entity_Spawners.md, решения пользователя
// 2026-09-20). Клетка больше не решает сама, кто на ней проявится: решает
// спавнер -- точка с радиусом. Он выбирает ОДИН вид по условиям карточки,
// прочитанным на клетке его центра, и выпускает до PackSize особей, которые
// бродят внутри его радиуса. Так кончаются «шахматы»: раньше каждая
// подходящая клетка заводила своё существо, и подходящий участок заполнялся
// сплошь.
//
// Спавнеры бывают двух видов, и это одна и та же структура состояния:
//   * автоматический -- считается по сетке квадратов со стороной
//     UHerbalistSettings::AmbientSpawnerSpacingMeters, детерминированно по
//     клетке (тот же приём «сиды от клетки», что уже у ресурсов), нигде не
//     хранится и не сохраняется;
//   * ручной -- актор AAmbientEntitySpawner, поставленный на уровень; в
//     своём радиусе он отменяет автоматические (bOverridesAuto).
#pragma once

#include "CoreMinimal.h"
#include "Core/Types/HerbalistCellCoord.h"

class AAmbientEntityActor;
class AAmbientEntitySpawner;

// Живое состояние одного спавнера. Не UPROPERTY-структура и не сохраняется:
// всё в ней -- производная от условий мира и сида (см. «Сейв» в
// DESIGN_Entity_Spawners.md).
struct FAmbientSpawnerRuntime
{
    // Клетка центра: по ней читаются условия карточек.
    FIntPoint CenterCell = HerbalistCore::InvalidCell();

    // Радиус зоны в сантиметрах (в настройках и на акторе -- метры).
    float RadiusCm = 2500.0f;

    // Вид, который спавнер держит сейчас. Пусто -- ни одна карточка не
    // подходит, зона пуста.
    FName ActiveEntityID = NAME_None;

    // Уже выпущенные особи. Слабые указатели -- тот же приём, что у
    // FGridCell::ManifestedEntityActor: актор может исчезнуть мимо нас.
    TArray<TWeakObjectPtr<AAmbientEntityActor>> Individuals;

    // Осталось секунд до следующего выпуска: стайка собирается постепенно.
    float SpawnCooldownSeconds = 0.0f;

    // Ручной спавнер, породивший это состояние (пусто -- автоматический).
    TWeakObjectPtr<AAmbientEntitySpawner> ManualSpawner;
};

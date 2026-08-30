// AmbientEntityActor.h
//
// Низший ранг бестиария (§16.2) как физический актор — 2026-08-30. Пустой
// маркерный под-класс: собственного поведения пока нет (см.
// HerbalistEntityActor.h), но нужен отдельный C++-тип, чтобы Низший ранг мог
// разойтись от Хозяев/Легендарных позже (декоративность, отсутствие
// взаимодействия по спецификации §16.2) без переписывания базового класса.
// Разворачивает решение "амбиентная зона, без актора" из
// 16_Entity_Manifestation.md — см. правку того файла в этом же коммите.
#pragma once

#include "CoreMinimal.h"
#include "Core/Entities/HerbalistEntityActor.h"
#include "AmbientEntityActor.generated.h"

UCLASS()
class PROJECTHERBALIST_API AAmbientEntityActor : public AHerbalistEntityActor
{
    GENERATED_BODY()
};

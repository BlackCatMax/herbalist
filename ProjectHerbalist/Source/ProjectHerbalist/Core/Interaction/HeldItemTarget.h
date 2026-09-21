// HeldItemTarget.h
//
// «С предметом в руке к этому можно что-то сделать» (DESIGN_Diegetic_Interface.md,
// этап 3, 2026-09-21): котёл принимает траву и воду, тайник -- зелье, и так
// далее по таблице «предмет + цель». Отдельно от IInteractable: тот --
// взаимодействие пустой рукой (помешать котёл, открыть дверь), этот --
// применение того, что держишь. У одной цели бывают оба.
//
// Только C++: применение из руки -- слой ввода над симуляцией, Blueprint его
// не переопределяет.
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "HeldItemTarget.generated.h"

class AHerbalistPlayerController;

UINTERFACE(meta = (CannotImplementInterfaceInBlueprint))
class PROJECTHERBALIST_API UHeldItemTarget : public UInterface
{
    GENERATED_BODY()
};

class PROJECTHERBALIST_API IHeldItemTarget
{
    GENERATED_BODY()

public:
    // Предмет из ячейки InventoryIndex котомки поднесён к цели. true -- цель
    // его обработала (приняла или отказала, объяснив отказ); false -- цели
    // такой предмет ни к чему, взаимодействие идёт дальше, как пустой рукой.
    virtual bool ReceiveHeldItem(AHerbalistPlayerController* PC, int32 InventoryIndex) { return false; }
};

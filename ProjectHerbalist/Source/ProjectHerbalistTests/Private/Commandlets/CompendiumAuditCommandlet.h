// CompendiumAuditCommandlet.h
//
// Сверка компендиума (карточек-заметок) с DataTable-ассетами (2026-09-03,
// предложение пользователя написать парсеры). ТОЛЬКО ЧТЕНИЕ: коммандлет
// ничего не записывает ни в карточки, ни в таблицы -- сначала надо увидеть
// масштаб расхождения, а уже потом решать, кто кому источник истины.
//
// Почему не «сгенерировать таблицы из карточек» сразу: карточки несут
// идентичность и лор (оси, мета-параметры, биом, ранг), а таблицы вдобавок
// -- геймплейный тюнинг, которого в карточках нет вовсе (MorokThreshold,
// TriggerThreshold, HysteresisMargin, ставки эффектов, SortOrder с
// задокументированными тай-брейками, bLandOnly/bWaterOnly), и ссылки на
// ассеты, назначаемые в редакторе (ResourceMesh, ResourceActorClass).
// Генерация «в лоб» уничтожила бы и то и другое.
//
// Три существующих extract_*.py в корне репозитория парсят тот же
// фронтматтер в JSON под ручной Import через Content Browser -- но такой
// импорт ЗАМЕНЯЕТ таблицу целиком, то есть ровно тот способ потерять
// тюнинг. Этот коммандлет читает и карточки, и сами .uasset, поэтому может
// сравнивать поле с полем.
//
// Запуск: UnrealEditor-Cmd.exe <uproject> -run=CompendiumAudit
//         (необязательно -CompendiumPath=<путь к 04_Compendium>)
#pragma once

#include "CoreMinimal.h"
#include "Commandlets/Commandlet.h"
#include "CompendiumAuditCommandlet.generated.h"

UCLASS()
class UCompendiumAuditCommandlet : public UCommandlet
{
    GENERATED_BODY()

public:
    virtual int32 Main(const FString& Params) override;
};

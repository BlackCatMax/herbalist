// Реализация модуля тестов.
//
// Нужна именно потому, что модуль объявлен в ProjectHerbalist.uproject: без
// IMPLEMENT_MODULE движок грузит DLL, но не может её инициализировать
// («The game module 'ProjectHerbalistTests' could not be successfully
// initialized after it was loaded»), и тесты не регистрируются.
//
// Собственной логики у модуля нет — только автотесты в соседних файлах,
// которые регистрируются через IMPLEMENT_SIMPLE_AUTOMATION_TEST при загрузке.
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, ProjectHerbalistTests);

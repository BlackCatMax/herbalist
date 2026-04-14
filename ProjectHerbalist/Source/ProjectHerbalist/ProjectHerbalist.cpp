#include "ProjectHerbalist.h"
#include "Modules/ModuleManager.h"

// 👇 ВАЖНО: подключаем, где DECLARE_LOG_CATEGORY_EXTERN
#include "Core/Pipeline/HerbalistPipeline.h"

DEFINE_LOG_CATEGORY(LogHerbalist);

IMPLEMENT_PRIMARY_GAME_MODULE(FDefaultGameModuleImpl, ProjectHerbalist, "ProjectHerbalist");
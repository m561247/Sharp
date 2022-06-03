//
// Created by bnunnally on 8/27/21.
//

#ifndef SHARP_COMPILER_INFO_H
#define SHARP_COMPILER_INFO_H

#include "List.h"
#include "frontend/ErrorManager.h"
#include "backend/dependency/component_manager.h"
#include "taskdelegator/task_delegator.h"
#include <atomic>

#define PROG_NAME "sharp"
#define PROG_VERS "0.3.0"

#define opt(v) strcmp(args[i], v) == 0

struct sharp_file;
struct sharp_module;
struct sharp_class;

// globaly used vars
extern bool panic;
extern List<sharp_file*> sharpFiles;
extern List<sharp_module*> modules;
extern List<sharp_class*> classes;
extern List<sharp_class*> genericClasses;
extern recursive_mutex globalLock;
extern thread_local sharp_module* currModule;
extern component_manager componentManager;
extern atomic<uInt> uniqueId;

#define global_class_name "__srt_global"
#define main_component_name "__main__"
#define instance_init_name(name) ("init<" + (name) + ">")
#define static_init_name(name)  ("static_init<" + (name) + ">")
#define any_component_name "?"
#define single_component_field_name_prefix "@sub_component_"
#define anonymous_func_prefix "@anonymous_fun_"
#define current_file currThread->currTask->file
#define error_manager currThread->currTask->file->errors
#define curr_context currThread->currTask->file->context

#define GUARD(mut) \
    std::lock_guard<recursive_mutex> guard(mut);

#define GUARD2(mut) \
    std::lock_guard<recursive_mutex> guard2(mut);

void create_new_warning(error_type error, int type, int line, int col, string xcmnts);
void create_new_warning(error_type error, int type, Ast *ast, string xcmnts);
bool all_files_parsed();

template<class T>
static void deleteList(List<T> &lst)
{
    for(unsigned int i = 0; i < lst.size(); i++)
    {
        delete lst.get(i);
    }

    lst.free();
}

#endif //SHARP_COMPILER_INFO_H

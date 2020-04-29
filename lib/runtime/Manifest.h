//
// Created by BraxtonN on 2/15/2018.
//

#ifndef SHARP_MANIFEST_H
#define SHARP_MANIFEST_H

#include "../../stdimports.h"
#include "oo/string.h"
#include "List.h"

using namespace runtime;

/**
 * Application info
 */
struct Manifest {
public:
    Manifest()
    {
        executable.init();
        application.init();
        version.init();
        debug = false;
        entryMethod = 0;
        methods = 0;
        classes = 0;
        fvers = 0;
        target = 0;
        sourceFiles = 0;
        strings = 0;
        constants = 0;
        threadLocals = 0;
    }

    String executable;
    String application;
    String version;
    bool debug;
    Int entryMethod;
    Int methods, classes;
    Int fvers;
    Int target;
    uInt sourceFiles;
    Int strings;
    Int constants;
    Int threadLocals;
};

struct SourceFile {
    Int id;
    String name;
    _List<String> lines;

    void init() {
        id = 0;
        name.init();
        lines.init();
    }

    void free() {
        for(uInt i = 0; i < lines.size(); i++)
            lines.get(i).free();
        lines.free();
    }
};

/**
 * File and debugging info, line
 * by line debugging meta data
 */
class Meta {

public:
    Meta()
            :
            files()
    {
    }

    void init() { Meta(); }

    _List<SourceFile> files;

    String getLine(uInt line, uInt sourceFile);
    bool hasLine(uInt line, uInt sourceFile);
    void free();
};


#endif //SHARP_MANIFEST_H

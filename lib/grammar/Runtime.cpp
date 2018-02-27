//
// Created by bknun on 9/12/2017.
//

#include "Runtime.h"
#include "parser/ErrorManager.h"
#include "List.h"
#include "parser/Parser.h"
#include "../util/File.h"
#include "../runtime/Opcode.h"
#include "../runtime/register.h"

using namespace std;

unsigned long RuntimeEngine::uniqueSerialId = 0;
options c_options;

void help();

int _bootstrap(int argc, const char* argv[])
{
    if(argc < 2) {
        help();
        return 1;
    }


    initalizeErrors();
    List<string> files;
    for (int i = 1; i < argc; ++i) {
        args_:
        if(opt("-a")){
            c_options.aggressive_errors = true;
        }
        else if(opt("-c")){
            c_options.compile = true;
        }
        else if(opt("-o")){
            if(i+1 >= argc)
                rt_error("output file required after option `-o`");
            else
                c_options.out = string(argv[++i]);
        }
        else if(opt("-V")){
            printVersion();
            exit(0);
        }
        else if(opt("-O")){
            c_options.optimize = true;
        }
        else if(opt("-h") || opt("-?")){
            help();
            exit(0);
        }
        else if(opt("-R") || opt("-release")){
            c_options.optimize = true;
            c_options.debug = false;
            c_options.strip = true;
        }
        else if(opt("-s")){
            c_options.strip = true;
        }
        else if(opt("-magic")){
            c_options.magic = true;
        }
        else if(opt("-debug")) {
            c_options.debugMode = true;
        }
        else if(opt("-showversion")){
            printVersion();
            cout << endl;
        }
        else if(opt("-target")){
            if(i+1 >= argc)
                rt_error("file version required after option `-target`");
            else {
                std::string x = std::string(argv[++i]);
                if(all_integers(x))
                    c_options.target = strtol(x.c_str(), NULL, 0);
                else {
                    if(to_lower(x) == "base") {
                        c_options.target = versions.BASE;
                    } else if(to_lower(x) == "alpha") {
                        c_options.target = versions.ALPHA;
                    }
                    else {
                        rt_error("unknown target " + x);
                    }
                }
            }
        }
        else if(opt("-w")){
            c_options.warnings = false;
        }
        else if(opt("-v")){
            if(i+1 >= argc)
                rt_error("file version required after option `-v`");
            else
                c_options.vers = string(argv[++i]);
        }
        else if(opt("-u") || opt("-unsafe")){
            c_options.unsafe = true;
        }
        else if(opt("-werror")){
            c_options.werrors = true;
            c_options.warnings = true;
        }
        else if(opt("-errlmt")) {
            std::string lmt = std::string(argv[++i]);
            if(all_integers(lmt)) {
                c_options.error_limit = strtoul(lmt.c_str(), NULL, 0);

                if(c_options.error_limit > 100000) {
                    rt_error("cannot set the max errors allowed higher than (100,000) - " + lmt);
                } else if(c_options.error_limit == 0) {
                    rt_error("cannot have an error limit of 0 ");
                }
            }
            else {
                rt_error("invalid error limit set " + lmt);
            }
        }
        else if(opt("-objdmp")){
            c_options.objDump = true;
        }
        else if(string(argv[i]).at(0) == '-'){
            rt_error("invalid option `" + string(argv[i]) + "`, try bootstrap -h");
        }
        else {
            // add the source files
            do {
                if(string(argv[i]).at(0) == '-')
                    goto args_;

                files.addif(string(argv[i++]));
            }while(i<argc);
            break;
        }
    }

    if(files.size() == 0){
        help();
        return 1;
    }

    for(unsigned int i = 0; i < files.size(); i++) {
        string& file = files.get(i);

        if(!File::exists(file.c_str())){
            rt_error("file `" + file + "` doesnt exist!");
        }
        if(!File::endswith(".sharp", file)){
            rt_error("file `" + file + "` is not a sharp file!");
        }
    }

    exec_runtime(files);
    return 0;
}

void help() {
    cout << "Usage: bootstrap" << "{OPTIONS} SOURCE FILE(S)" << std::endl;
    cout << "Source file must have a .sharp extion to be compiled.\n" << endl;
    cout << "[-options]\n\n    -V                print compiler version and exit"                   << endl;
    cout <<               "    -showversion      print compiler version and continue"               << endl;
    cout <<               "    -o<file>          set the output object file"                        << endl;
    cout <<               "    -c                compile only and do not generate exe"              << endl;
    cout <<               "    -a                enable aggressive error reporting"                 << endl;
    cout <<               "    -s                string debugging info"                             << endl;
    cout <<               "    -O                optimize executable"                               << endl;
    cout <<               "    -w                disable all warnings"                              << endl;
    cout <<               "    -errlmt<count>    set max errors the compiler allows before quitting"  << endl;
    cout <<               "    -v<version>       set the application version"                       << endl;
    cout <<               "    -unsafe -u        allow unsafe code"                                 << endl;
    cout <<               "    -objdmp           create dump file for generated assembly"           << endl;
    cout <<               "    -target           target the specified platform of sharp to run on"  << endl;
    cout <<               "    -werror           enable warnings as errors"                         << endl;
    cout <<               "    -release -r       generate a release build exe"                      << endl;
    cout <<               "    --h -?            display this help message"                         << endl;
}

void rt_error(string message) {
    cout << "bootstrap:  error: " << message << endl;
    exit(1);
}

void printVersion() {
    cout << progname << " " << progvers;
}

std::string to_lower(string s) {
    string newstr = "";
    for(char c : s) {
        newstr += tolower(c);
    }
    return newstr;
}

bool all_integers(string int_string) {
    for(char c : int_string) {
        if(!isdigit(c))
            return false;
    }
    return true;
}

void exec_runtime(List<string>& files)
{
    List<Parser*> parsers;
    Parser* p = NULL;
    tokenizer* t;
    File::buffer source;
    size_t errors=0, unfilteredErrors=0;
    long succeeded=0, failed=0, panic=0;

    for(unsigned int i = 0; i < files.size(); i++) {
        string& file = files.get(i);
        source.begin();

        File::read_alltext(file.c_str(), source);
        if(source.empty()) {
            for(unsigned long i = 0; i < parsers.size(); i++) {
                Parser* parser = parsers.get(i);
                parser->free();
                delete(parser);
            }

            rt_error("file `" + file + "` is empty.");
        }

        if(c_options.debugMode)
            cout << "tokenizing " << file << endl;

        t = new tokenizer(source.to_str(), file);
        if(t->getErrors()->hasErrors())
        {
            t->getErrors()->printErrors();

            errors+= t->getErrors()->getErrorCount();
            unfilteredErrors+= t->getErrors()->getUnfilteredErrorCount();
            failed++;
        }
        else {
            if(c_options.debugMode)
                cout << "parsing " << file << endl;

            p = new Parser(t);
            parsers.push_back(p);

            if(p->getErrors()->hasErrors())
            {
                p->getErrors()->printErrors();

                errors+= p->getErrors()->getErrorCount();
                unfilteredErrors+= p->getErrors()->getUnfilteredErrorCount();
                failed++;

                if(p->panic) {
                    panic = 1;
                    goto end;
                }

            } else {
                succeeded++;
            }
        }

        end:
        t->free();
        delete (t);
        source.end();

        if(panic==1) {
            cout << "Detected more than " << c_options.error_limit << "+ errors, quitting.";
            break;
        }
    }

    if(!panic && errors == 0 && unfilteredErrors == 0) {
        if(c_options.debugMode)
            cout << "preparing to perform syntax analysis on project files"<< endl;

        RuntimeEngine engine(c_options.out, parsers);

        failed = engine.failedParsers.size();
        succeeded = engine.succeededParsers.size();

        errors+=engine.errorCount;
        unfilteredErrors+=engine.unfilteredErrorCount;
        if(errors == 0 && unfilteredErrors == 0) {
            if(!c_options.compile)
                engine.generate();
        }

        engine.cleanup();
    }
    else {
        for(unsigned long i = 0; i < parsers.size(); i++) {
            Parser* parser = parsers.get(i);
            parser->free();
            delete(parser);
        }
        parsers.free();
    }

    cout << endl << "==========================================================\n" ;
    cout << "Errors: " << (c_options.aggressive_errors ? unfilteredErrors : errors) << " Succeeded: "
         << succeeded << " Failed: " << failed << " Total: " << files.size() << endl;
}

void RuntimeEngine::compile()
{
    if(preprocess()) {
        resolveAllFields();
        resolveAllMethods();

        if(c_options.magic) {
            List<string> lst;
            lst.addAll(modules);

            for(unsigned long i = 0; i < parsers.size(); i++) {
                Parser* p = parsers.get(i);
                bool found = false;

                for(unsigned int i = 0; i < importMap.size(); i++) {
                    if(importMap.get(i).key == p->sourcefile) {
                        importMap.get(i).value.addAll(modules);
                        found = true;
                        break;
                    }
                }

                if(!found) {
                    importMap.add(KeyPair<std::string, List<std::string>>(p->sourcefile, lst));
                }
            }

            lst.free();
        }


        for(unsigned long i = 0; i < parsers.size(); i++) {
            Parser *p = parsers.get(i);
            activeParser = p;
            currentModule = "$unknown";

            errors = new ErrorManager(p->lines, p->sourcefile, true, c_options.aggressive_errors);

            Ast* ast;
            addScope(Scope(GLOBAL_SCOPE, NULL));
            for(size_t i = 0; i < p->treesize(); i++) {
                ast = p->ast_at(i);
                SEMTEX_CHECK_ERRORS

                switch(ast->getType()) {
                    case ast_module_decl:
                        currentModule = parseModuleName(ast);
                        break;
                    case ast_import_decl:
                        analyzeImportDecl(ast);
                        break;
                    case ast_class_decl:
                        analyzeClassDecl(ast);
                        break;
                    default:
                        stringstream err;
                        err << ": unknown ast type: " << ast->getType();
                        errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
                        break;
                }

            }

            if(errors->getErrorCount() == 0 && errors->getUnfilteredErrorCount() == 0) {
                getMainMethod(p);
            }

            if(errors->hasErrors()){
                report:

                errorCount+= errors->getErrorCount();
                unfilteredErrorCount+= errors->getUnfilteredErrorCount();

                failedParsers.addif(activeParser->sourcefile);
                succeededParsers.removefirst(activeParser->sourcefile);
            } else {
                failedParsers.addif(activeParser->sourcefile);
                succeededParsers.removefirst(activeParser->sourcefile);
            }

            removeScope();
            errors->free();
            delete (errors); this->errors = NULL;
        }
    }
}

void RuntimeEngine::setHeadClass(ClassObject *klass) {
    ClassObject* sup = klass->getBaseClass();
    if(sup == NULL) {
        klass->setHead(klass);
        return;
    }
    while(true) {
        if(sup->getBaseClass() == NULL) {
            klass->setHead(sup);
            return;
        } else
            sup = sup->getBaseClass();
    }
}

void RuntimeEngine::analyzeClassDecl(Ast *ast) {
    Scope* scope = currentScope();
    Ast* block = ast->getSubAst(ast_block);
    List<AccessModifier> modifiers;
    ClassObject* klass;
    int startpos=1;

    parseAccessDecl(ast, modifiers, startpos);
    string className =  ast->getEntity(startpos).getToken();

    if(scope->type == GLOBAL_SCOPE) {
        klass = getClass(currentModule, className);

        setHeadClass(klass);
    }
    else {
        klass = scope->klass->getChildClass(className);

        klass->setSuperClass(scope->klass);
        setHeadClass(klass);
    }

    addScope(Scope(CLASS_SCOPE, klass));
    for(long i = 0; i < block->getSubAstCount(); i++) {
        ast = block->getSubAst(i);
        CHECK_ERRORS

        switch(ast->getType()) {
            case ast_class_decl:
                analyzeClassDecl(ast);
                break;
            case ast_var_decl:
                analyzeVarDecl(ast);
                break;
            case ast_method_decl:
                parseMethodDecl(ast);
                break;
            case ast_operator_decl:
                parseOperatorDecl(ast);
                break;
            case ast_construct_decl:
                parseConstructorDecl(ast);
                break;
            default: {
                stringstream err;
                err << ": unknown ast type: " << ast->getType();
                errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
                break;
            }
        }
    }

    removeScope();
}

void RuntimeEngine::parseCharLiteral(token_entity token, Expression& expression) {
    expression.type = expression_var;

    int64_t  i64;
    if(token.getToken().size() > 1) {
        switch(token.getToken().at(1)) {
            case 'n':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\n'), ebx);
                expression.value = '\n';
                break;
            case 't':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\t'), ebx);
                expression.intValue = '\t';
                break;
            case 'b':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\b'), ebx);
                expression.intValue = '\b';
                break;
            case 'v':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\v'), ebx);
                expression.intValue = '\v';
                break;
            case 'r':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\r'), ebx);
                expression.intValue = '\r';
                break;
            case 'f':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\f'), ebx);
                expression.intValue = '\f';
                break;
            case '\\':
                expression.code.push_i64(SET_Di(i64, op_MOVI, '\\'), ebx);
                expression.intValue = '\\';
                break;
            default:
                expression.code.push_i64(SET_Di(i64, op_MOVI, token.getToken().at(1)), ebx);
                expression.intValue = token.getToken().at(1);
                break;
        }
    } else {
        expression.code.push_i64(SET_Di(i64, op_MOVI, token.getToken().at(0)), ebx);
        expression.intValue =token.getToken().at(0);
    }

    expression.literal = true;
}

string RuntimeEngine::invalidateUnderscores(string basic_string) {
    stringstream newstring;
    for(char c : basic_string) {
        if(c != '_')
            newstring << c;
    }
    return newstring.str();
}


void RuntimeEngine::parseIntegerLiteral(token_entity token, Expression& expression) {
    expression.type = expression_var;

    int64_t i64;
    double var;
    string int_string = invalidateUnderscores(token.gettoken());

    if(all_integers(int_string)) {
        var = std::strtod (int_string.c_str(), NULL);
        if(var > DA_MAX || var < DA_MIN) {
            stringstream ss;
            ss << "integral number too large: " + int_string;
            errors->createNewError(GENERIC, token.getline(), token.getcolumn(), ss.str());
        }
        expression.code.push_i64(SET_Di(i64, op_MOVI, var), ebx);
    }else {
        var = std::strtod (int_string.c_str(), NULL);
        if((int64_t )var > DA_MAX || (int64_t )var < DA_MIN) {
            stringstream ss;
            ss << "integral number too large: " + int_string;
            errors->createNewError(GENERIC, token.getline(), token.getcolumn(), ss.str());
        }

        expression.code.push_i64(SET_Di(i64, op_MOVBI, ((int64_t)var)), abs(get_low_bytes(var)));
        expression.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, bmr));
    }

    expression.intValue = var;
    expression.literal = true;
}

void RuntimeEngine::parseHexLiteral(token_entity token, Expression& expression) {
    expression.type = expression_var;

    int64_t i64;
    double var;
    string hex_string = invalidateUnderscores(token.gettoken());

    var = strtoll(hex_string.c_str(), NULL, 16);
    if(var > DA_MAX || var < DA_MIN) {
        stringstream ss;
        ss << "integral number too large: " + hex_string;
        errors->createNewError(GENERIC, token.getline(), token.getcolumn(), ss.str());
    }
    expression.code.push_i64(SET_Di(i64, op_MOVI, var), ebx);

    expression.intValue = var;
    expression.literal = true;
}

void RuntimeEngine::parseStringLiteral(token_entity token, Expression& expression) {
    expression.type = expression_string;

    string parsed_string = "";
    bool slash = false;
    for(char c : token.gettoken()) {
        if(slash) {
            slash = false;
            switch(c) {
                case 'n':
                    parsed_string += '\n';
                    break;
                case 't':
                    parsed_string += '\t';
                    break;
                case 'b':
                    parsed_string += '\b';
                    break;
                case 'v':
                    parsed_string += '\v';
                    break;
                case 'r':
                    parsed_string += '\r';
                    break;
                case 'f':
                    parsed_string += '\f';
                    break;
                case '\\':
                    parsed_string += '\\';
                    break;
                default:
                    parsed_string += c;
                    break;
            }
        }

        if(c == '\\') {
            slash = true;
        } else {
            parsed_string += c;
        }
    }

    expression.value = parsed_string;
    stringMap.addif(parsed_string);
    expression.intValue = stringMap.indexof(parsed_string);
}

void RuntimeEngine::parseBoolLiteral(token_entity token, Expression& expression) {
    expression.type = expression_var;
    expression.code.push_i64(SET_Di(i64, op_MOVI, (token.gettoken() == "true" ? 1 : 0)), ebx);

    expression.intValue = (token.gettoken() == "true" ? 1 : 0);
    expression.literal = true;
}

Expression RuntimeEngine::parseLiteral(Ast* pAst) {
    Expression expression;

    switch(pAst->getEntity(0).getId()) {
        case CHAR_LITERAL:
            parseCharLiteral(pAst->getEntity(0), expression);
            break;
        case INTEGER_LITERAL:
            parseIntegerLiteral(pAst->getEntity(0), expression);
            break;
        case HEX_LITERAL:
            parseHexLiteral(pAst->getEntity(0), expression);
            break;
        case STRING_LITERAL:
            parseStringLiteral(pAst->getEntity(0), expression);
            break;
        default:
            if(pAst->getEntity(0).getToken() == "true" ||
               pAst->getEntity(0).getToken() == "false") {
                parseBoolLiteral(pAst->getEntity(0), expression);
            }
            break;
    }
    expression.link = pAst;
    return expression;
}

Expression RuntimeEngine::psrseUtypeClass(Ast* pAst) {
    Expression expression = parseUtype(pAst);

    if(pAst->hasentity(DOT)) {
        expression.dot = true;
    }

    if(expression.type == expression_class) {
        expression.code.push_i64(SET_Di(i64, op_MOVI, expression.utype.klass->address), ebx);
    } else {
        errors->createNewError(GENERIC, pAst->getSubAst(ast_utype)->getSubAst(ast_type_identifier)->line,
                         pAst->getSubAst(ast_utype)->getSubAst(ast_type_identifier)->col, "expected class");
    }
    expression.link = pAst;
    return expression;
}

List<Expression> RuntimeEngine::parseValueList(Ast* pAst) {
    List<Expression> expressions;

    Ast* vAst;
    for(unsigned int i = 0; i < pAst->getSubAstCount(); i++) {
        vAst = pAst->getSubAst(i);
        expressions.add(parseValue(vAst));
    }

    return expressions;
}

bool RuntimeEngine::expressionListToParams(List<Param> &params, List<Expression>& expressions) {
    bool success = true;
    Expression* expression;
    List<AccessModifier> mods;
    RuntimeNote note;
    Field field;

    for(unsigned int i = 0; i < expressions.size(); i++) {
        expression = &expressions.get(i);
        if(expression->link != NULL) {
            note = RuntimeNote(activeParser->sourcefile, activeParser->geterrors()->getline(expression->link->line), expression->link->line, expression->link->col);
        }

        if(expression->type == expression_var) {
            field = Field(VAR, 0, "", NULL, mods, note);
            field.isArray = expression->utype.array;

            params.add(Param(field));
        } else if(expression->type == expression_string) {
            field = Field(VAR, 0, "", NULL, mods, note);
            field.isArray = true;

            /* Native string is a char array */
            params.add(Param(field));
        } else if(expression->type == expression_class) {
            success = false;
            errors->createNewError(INVALID_PARAM, expression->link->line, expression->link->col, " `class`, param must be lvalue");
        } else if(expression->type == expression_lclass) {
            field = Field(expression->utype.klass, 0, "", NULL, mods, note);
            field.type = CLASS;
            field.isArray = expression->utype.array;

            params.add(Param(field));
        } else if(expression->type == expression_field) {
            params.add(*expression->utype.field);
        } else if(expression->type == expression_native) {
            success = false;
            errors->createNewError(GENERIC, expression->link->line, expression->link->col, " unexpected symbol `" + expression->utype.refrenceName + "`");
        } else if(expression->type == expression_objectclass) {
            field = Field(OBJECT, 0, "", NULL, mods, note);
            field.isArray = expression->utype.array;

            params.add(field);
        } else if(expression->type == expression_void) {
            success = false;
            errors->createNewError(GENERIC, expression->link->line, expression->link->col, " expression of type `void` cannot be a param");
        }
        else if(expression->type == expression_null) {
            field = Field(OBJECT, 0, "", NULL, mods, note);
            field.nullType = true;

            params.add(Param(field));
        } else {
            /* Unknown expression */
            field = Field(UNDEFINED, 0, "", NULL, mods, note);

            params.add(Param(field));
            success = false;
        }
    }

    return success;
}

/**
 * This gets the name from the dotted refrence
 * Main.foo foo will be split from the dot notation to lookup the
 * classes before it
 * @param name
 * @param ptr
 * @return
 */
bool RuntimeEngine::splitMethodUtype(string& name, ReferencePointer& ptr) {
    if(ptr.singleRefrence() || ptr.singleRefrenceModule()) {
        return false;
    } else {
        name = ptr.referenceName;
        ptr.referenceName = ptr.classHeiarchy.last(); // assign the last field in the accessor to class
        ptr.classHeiarchy.remove(ptr.classHeiarchy.size()-1);
        return true;
    }
}

string RuntimeEngine::paramsToString(List<Param> &param) {
    string message = "(";
    for(unsigned int i = 0; i < param.size(); i++) {
        switch(param.get(i).field.type) {
            case VAR:
                message += "var";
                break;
            case CLASS:
                if(param.get(i).field.nullType) {
                    if(param.get(i).field.klass == NULL)
                        message += "<class-type>";
                    else
                        message += param.get(i).field.klass->getName();
                } else message += param.get(i).field.klass->getFullName();
                break;
            case OBJECT:
                message += "object";
                break;
            default:
                message += "?";
                break;
        }

        if(param.get(i).field.isArray)
            message += "[]";
        if((i+1) < param.size()) {
            message += ",";
        }
    }

    message += ")";
    return message;
}

void RuntimeEngine::pushAuthenticExpressionToStackNoInject(Expression& expression, Expression& out) {

    switch(expression.type) {
        case expression_var:
            if(expression.newExpression) {
            } else {
                if (expression.func) {
                    out.code.push_i64(SET_Di(i64, op_INC, sp));
                } else
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
            }
            break;
        case expression_field:
            if(expression.utype.field->isVar() && !expression.utype.field->isArray) {
                if(expression.utype.field->local) {
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                } else {
                    out.code.push_i64(SET_Di(i64, op_MOVI, 0), adx);
                    out.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, adx));
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                }
            } else if(expression.utype.field->isVar() && expression.utype.field->isArray) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            } else if(expression.utype.field->dynamicObject() || expression.utype.field->type == CLASS) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }
            break;
        case expression_lclass:
            if(expression.newExpression) {
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                    out.code.push_i64(SET_Di(i64, op_INC, sp));
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
        case expression_string:
            out.code.push_i64(SET_Di(i64, op_NEWSTRING, expression.intValue));
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            break;
        case expression_null:
            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Ei(i64, op_DEL));
            break;
        case expression_objectclass:
            if(expression.newExpression) {
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                    out.code.push_i64(SET_Di(i64, op_INC, sp));
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
    }
}

void RuntimeEngine::pushExpressionToPtr(Expression& expression, Expression& out) {
    if(!expression.newExpression)
        out.code.inject(out.code.__asm64.size(), expression.code);

    switch(expression.type) {
        case expression_var:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            }

            if(expression.func) {
                out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            }
            break;
        case expression_field:
            break;
        case expression_lclass:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                    out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
                }
            }
            break;
        case expression_string:
            out.code.push_i64(SET_Di(i64, op_MOVSL, 1));
            out.code.push_i64(SET_Di(i64, op_NEWSTRING, expression.intValue));
            break;
        case expression_null:
            out.code.push_i64(SET_Di(i64, op_MOVSL, 1));
            out.code.push_i64(SET_Ei(i64, op_DEL));
            break;
        case expression_objectclass:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                    out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
                }
            }
            break;
    }
}

void RuntimeEngine::pushExpressionToStack(Expression& expression, Expression& out) {
    if(!expression.newExpression)
        out.code.inject(out.code.__asm64.size(), expression.code);

    switch(expression.type) {
        case expression_var:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
            } else {
                if (expression.func) {
                } else
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
            }
            break;
        case expression_field:
            if(expression.utype.field->isVar() && !expression.utype.field->isArray) {
                if(expression.utype.field->local) {
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                } else {
                    out.code.push_i64(SET_Di(i64, op_MOVI, 0), adx);
                    out.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, adx));
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                }
            } else if(expression.utype.field->isVar() && expression.utype.field->isArray) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            } else if(expression.utype.field->dynamicObject() || expression.utype.field->type == CLASS) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }
            break;
        case expression_lclass:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
        case expression_string:
            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Di(i64, op_NEWSTRING, expression.intValue));
            break;
        case expression_null:
            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Ei(i64, op_DEL));
            break;
        case expression_objectclass:
            if(expression.newExpression) {
                out.code.inject(out.code.__asm64.size(), expression.code);
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
    }
}

Method* RuntimeEngine::resolveMethodUtype(Ast* utype, Ast* valueLst, Expression &out) {
    Scope* scope = currentScope();
    Method* fn = NULL;
    utype = utype->getSubAst(ast_utype);
    valueLst = valueLst->getSubAst(ast_value_list);

    /* This is a naked utype so we dont haave to worry about brackets */
    ReferencePointer ptr;
    string methodName;
    List<Param> params;
    List<Expression> expressions = parseValueList(valueLst);
    Expression expression;
    bool access = false;

    expressionListToParams(params, expressions);
    ptr = parseTypeIdentifier(utype);

    if(splitMethodUtype(methodName, ptr)) {
        access = true;
        // accessor
        resolveUtype(ptr, expression, utype);
        if(expression.type == expression_class || (expression.type == expression_field && expression.utype.field->type == CLASS)) {
            ClassObject* klass;
            if(expression.type == expression_class) {
                klass = expression.utype.klass;
            } else {
                klass = expression.utype.field->klass;
            }

            if((fn = klass->getFunction(methodName, params, true)) != NULL){}
            else if((fn = klass->getOverload(stringToOp(methodName), params, true)) != NULL){}
            else if((fn = klass->getConstructor(params)) != NULL) {}
            else if(klass->getField(methodName, true) != NULL) {
                errors->createNewError(GENERIC, valueLst->line, valueLst->col, " symbol `" + methodName + "` is a field");
            }
            else {
                if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + methodName;

                errors->createNewError(COULD_NOT_RESOLVE, valueLst->line, valueLst->col, " `" + methodName + paramsToString(params) + "`");
            }
        } else if(expression.type == expression_field && expression.utype.field->type != CLASS) {
            errors->createNewError(GENERIC, expression.lnk->line, expression.lnk->col, " field `" +  expression.utype.field->name + "` is not a class");

        } else if(expression.utype.type == UNDEFINED) {
        }
        else {
            errors->createNewError(COULD_NOT_RESOLVE, valueLst->line, valueLst->col, " `" +  methodName + paramsToString(params) +  "`");
        }
    } else {
        // method or global macros
        if(ptr.singleRefrence()) {
            if(scope->type == GLOBAL_SCOPE) {
                errors->createNewError(COULD_NOT_RESOLVE, valueLst->line, valueLst->col, " `" + ptr.referenceName +  paramsToString(params) + "`");
            } else {
                
                if((fn = scope->klass->getFunction(ptr.referenceName, params, true)) != NULL){}
                else if((fn = scope->klass->getOverload(stringToOp(ptr.referenceName), params, true)) != NULL){}
                else if(ptr.referenceName == scope->klass->getName() && (fn = scope->klass->getConstructor(params)) != NULL) {}
                else if(scope->klass->getField(methodName, true) != NULL) {
                    errors->createNewError(GENERIC, valueLst->line, valueLst->col, " symbol `" + methodName + "` is a field");
                }
                else {
                    if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + ptr.referenceName;
                    else methodName = ptr.referenceName;
                    errors->createNewError(COULD_NOT_RESOLVE, valueLst->line, valueLst->col, " `" + ptr.referenceName +  paramsToString(params) + "`");
                }
            }

            if(fn != NULL && !fn->isStatic()) {
                expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
            }
        } else {
            errors->createNewError(COULD_NOT_RESOLVE, valueLst->line, valueLst->col, " `" + ptr.referenceName +  paramsToString(params) + "`");
        }
    }


    if(fn != NULL) {

        if(fn->type != TYPEVOID && fn->nativeParamCount() == 0)
            out.code.push_i64(SET_Di(i64, op_INC, sp));

        if(!fn->isStatic()) {
            if(access && scope->last_statement == ast_return_stmnt) {
                out.inject(expression);
                pushAuthenticExpressionToStackNoInject(expression, out);
            } else {

                pushExpressionToPtr(expression, out);
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }
        }

        for(unsigned int i = 0; i < expressions.size(); i++) {
            pushExpressionToStack(expressions.get(i), out);
        }
        out.code.push_i64(SET_Di(i64, op_CALL, fn->address));
    }

    freeList(params);
    freeList(expressions);
    ptr.free();
    return fn;
}

expression_type RuntimeEngine::methodReturntypeToExpressionType(Method* fn) {
    if(fn->type == CLASS)
        return expression_lclass;
    else if(fn->type == OBJECT) {
        return expression_objectclass;
    } else if(fn->type == VAR) {
        return expression_var;
    } else if(fn->type == TYPEVOID)
        return expression_void;
    else
        return expression_unknown;
}

void RuntimeEngine::resolveUtypeContext(ClassObject* classContext, ReferencePointer& refrence, Expression& expression, Ast* pAst) {
    int64_t i64;
    Field* field;

    if(refrence.singleRefrence()) {
        if((field = classContext->getField(refrence.referenceName)) != NULL) {

            expression.utype.type = CLASSFIELD;
            expression.utype.field = field;
            expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));
            expression.type = expression_field;
        }else {
            /* Un resolvable */
            errors->createNewError(COULD_NOT_RESOLVE, pAst->line, pAst->col, " `" + refrence.referenceName + "` " + (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));

            expression.utype.type = UNDEFINED;
            expression.utype.referenceName = refrence.referenceName;
            expression.type = expression_unresolved;
        }
    } else {
        resolveClassHeiarchy(classContext, refrence, expression, pAst);
    }
}

void RuntimeEngine::pushExpressionToStackNoInject(Expression& expression, Expression& out) {

    switch(expression.type) {
        case expression_var:
            if(expression.newExpression) {
            } else {
                if (expression.func) {
                } else
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
            }
            break;
        case expression_field:
            if(expression.utype.field->isVar() && !expression.utype.field->isArray) {
                if(expression.utype.field->local) {
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                } else {
                    out.code.push_i64(SET_Di(i64, op_MOVI, 0), adx);
                    out.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, adx));
                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                }
            } else if(expression.utype.field->isVar() && expression.utype.field->isArray) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            } else if(expression.utype.field->dynamicObject() || expression.utype.field->type == CLASS) {
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }
            break;
        case expression_lclass:
            if(expression.newExpression) {
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
        case expression_string:
            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Di(i64, op_NEWSTRING, expression.intValue));
            break;
        case expression_null:
            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Ei(i64, op_DEL));
            break;
        case expression_objectclass:
            if(expression.newExpression) {
            } else {
                if(expression.func) {
                    /* I think we do nothing? */
                } else {
                    out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
                }
            }
            break;
    }
}


Method* RuntimeEngine::resolveContextMethodUtype(ClassObject* classContext, Ast* pAst, Ast* pAst2, Expression& out, Expression& contextExpression) {
    Scope* scope = currentScope();
    Method* fn = NULL;

    /* This is a naked utype so we dont haave to worry about brackets */
    ReferencePointer ptr;
    string methodName = "";
    List<Param> params;
    List<Expression> expressions = parseValueList(pAst2);
    Expression expression;

    expressionListToParams(params, expressions);
    ptr = parseTypeIdentifier(pAst);

    if(ptr.module != "") {
        errors->createNewError(GENERIC, pAst->line, pAst->col, "module name not allowed in nested method call");
        return NULL;
    }

    if(splitMethodUtype(methodName, ptr)) {
        // accessor
        resolveUtypeContext(classContext, ptr, expression, pAst);
        if(expression.type == expression_class || (expression.type == expression_field && expression.utype.field->type == field_class)) {
            ClassObject* klass;
            if(expression.type == expression_class) {
                klass = expression.utype.klass;
            } else {
                klass = expression.utype.field->klass;
            }

            if((fn = klass->getFunction(methodName, params)) != NULL){}
            else if((fn = klass->getOverload(stringToOp(methodName), params)) != NULL){}
            else if(classContext->getField(methodName) != NULL) {
                errors->createNewError(GENERIC, pAst2->line, pAst2->col, " symbol `" + classContext->getFullName() + methodName + "` is a field");
            }
            else {
                if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + methodName;

                errors->createNewError(COULD_NOT_RESOLVE, pAst2->line, pAst2->col, " `" + classContext->getFullName() + "." 
                                                                             + (klass == NULL ? ptr.referenceName : methodName) + paramsToString(params) + "`");
            }
        }
        else if(expression.type == expression_field && expression.utype.field->type != CLASS) {
            errors->createNewError(GENERIC, pAst2->line, pAst2->col, " field `" + (expression.utype.klass == NULL ? ptr.referenceName : methodName) + "` is not a class");

        }
        else if(expression.utype.type == UNDEFINED) {
        }
        else {
            errors->createNewError(COULD_NOT_RESOLVE, pAst2->line, pAst2->col, " `" + classContext->getFullName() + "." + (expression.utype.klass == NULL ? ptr.referenceName : methodName) + "`");
        }
    } else {
        // method or global macros
        if(ptr.singleRefrence()) {
            if((fn = classContext->getFunction(ptr.referenceName, params)) != NULL){}
            else if((fn = classContext->getOverload(stringToOp(ptr.referenceName), params)) != NULL){}
            else if(classContext->getField(ptr.referenceName) != NULL) {
                errors->createNewError(GENERIC, pAst2->line, pAst2->col, " symbol `" + ptr.referenceName + "` is a field");
            }
            else {
                if(stringToOp(ptr.referenceName) != oper_UNDEFINED) methodName = "operator" + ptr.referenceName;
                else methodName = ptr.referenceName;

                errors->createNewError(COULD_NOT_RESOLVE, pAst2->line, pAst2->col, " `" + classContext->getFullName() + "." + methodName +  paramsToString(params) + "`");
            }
        } else {
            // must be macros but it can be
            errors->createNewError(COULD_NOT_RESOLVE, pAst2->line, pAst2->col, " `" + ptr.referenceName + "`");
        }
    }


    if(fn != NULL) {
        if(fn->isStatic()) {
            createNewWarning(GENERIC, pAst->line, pAst->col, "instance access to static function");
        }

        pushExpressionToStackNoInject(contextExpression, out);

        for(unsigned int i = 0; i < expressions.size(); i++) {
            pushExpressionToStack(expressions.get(i), out);
        }
        out.code.push_i64(SET_Di(i64, op_CALL, fn->address));
    }

    freeList(params);
    freeList(expressions);
    ptr.free();
    return fn;
}

Expression RuntimeEngine::parseUtypeContext(ClassObject* classContext, Ast* pAst) {
    pAst = pAst->getSubAst(ast_utype);
    ReferencePointer ptr=parseTypeIdentifier(pAst);
    Expression expression;

    if(ptr.singleRefrence() && Parser::isnative_type(ptr.referenceName)) {
        errors->createNewError(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + ptr.referenceName + "`");

        expression.type = expression_unresolved;
        expression.link = pAst;
        ptr.free();
        return expression;
    }

    resolveUtypeContext(classContext, ptr, expression, pAst);

    if(pAst->hasentity(LEFTBRACE) && pAst->hasentity(RIGHTBRACE)) {
        expression.utype.array = true;
    }

    expression.link = pAst;
    expression.utype.referenceName = ptr.toString();
    ptr.free();
    return expression;
}

Expression RuntimeEngine::parseDotNotationCallContext(Expression& contextExpression, Ast* pAst) {
    string method_name="";
    Expression expression, interm;
    Method* fn;

    if(contextExpression.type == expression_objectclass) {
        errors->createNewError(GENERIC, pAst->line, pAst->col, "cannot infer function from object, did you forget to add a cast? i.e. ((SomeClass)dynamic_class)");
        expression.type = expression_unresolved;
        return expression;
    } else if(contextExpression.type == expression_unresolved) {
        expression.type= expression_unresolved;
        return expression;
    } else if(contextExpression.type != expression_lclass) {
        errors->createNewError(GENERIC, pAst->line, pAst->col, "expression does not return a class");
        expression.type = expression_unresolved;
        return expression;
    }

    ClassObject* klass = contextExpression.utype.klass;

    if(pAst->gettype() == ast_dot_fn_e) {
        fn = resolveContextMethodUtype(klass, pAst->getSubAst(ast_utype),
                                       pAst->getSubAst(ast_value_list), expression, contextExpression);
        if(fn != NULL) {
            expression.type = methodReturntypeToExpressionType(fn);
            if(expression.type == expression_lclass)
                expression.utype.klass = fn->klass;
            expression.utype.array = fn->array;

            // TODO: check for ++ and --
            // TODO: parse dot_notation_chain expression
        } else
            expression.type = expression_unresolved;
        expression.func=true;
    } else {
        if(contextExpression.func)
            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));

        /*
         * Nasty way to check which ast to pass
         * can we do this a better way?
         */
        if(pAst->gettype() == ast_utype)
            expression = parseUtypeContext(klass, pAst);
        else
            expression = parseUtypeContext(klass, pAst);
    }

    if(pAst->hasentity(DOT)) {
        expression.dot = true;
    }

    expression.link = pAst;
    return expression;
}

bool RuntimeEngine::currentRefrenceAffected(Expression& expr) {
    int opcode;
    for(unsigned int i = 0; i < expr.code.size(); i++) {
        opcode = GET_OP(expr.code.__asm64.get(i));

        switch(opcode) {
            case op_MOVL:
            case op_MOVSL:
            case op_MOVN:
            case op_MOVG:
            case op_MOVND:
                return true;
            default:
                break;
        }
    }

    return false;
}

void RuntimeEngine::pushExpressionToRegister(Expression& expr, Expression& out, int reg) {
    out.code.inject(out.code.__asm64.size(), expr.code);
    pushExpressionToRegisterNoInject(expr, out, reg);
}

void RuntimeEngine::pushExpressionToRegisterNoInject(Expression& expr, Expression& out, int reg) {
    switch(expr.type) {
        case expression_var:
            if(expr.utype.array) {
                errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from var[] expression");
            }

            if(expr.func) {
                out.code.push_i64(SET_Di(i64, op_LOADVAL, reg));
            } else if(reg != ebx) {
                out.code.push_i64(SET_Ci(i64, op_MOVR, reg,0, ebx));
            }
            break;
        case expression_field:
            if(expr.utype.field->isVar() && !expr.utype.field->isArray) {
                if(expr.utype.field->local) {
                    if(reg != ebx) {
                        out.code.push_i64(SET_Ci(i64, op_MOVR, reg,0, ebx));
                    }
                } else {
                    out.code.push_i64(SET_Di(i64, op_MOVI, 0), adx);
                    out.code.push_i64(SET_Ci(i64, op_IALOAD_2, reg,0, adx));
                }
            } else if(expr.utype.field->isVar() && expr.utype.field->isArray) {
                errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from " + expr.utype.field->name + "[] expression");
            } else if(expr.utype.field->dynamicObject() || expr.utype.field->type == CLASS) {
                errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from non integer type `object`");
            }
            break;
        case expression_lclass:
            errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from non integer type `" + expr.utype.klass->getFullName() +"`");
            break;
        case expression_string:
            errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from non integer type `var[]`");
            break;
        case expression_null:
            errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from type of null");
            break;
        case expression_objectclass:
            errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from non integer type `dynamic_object`");
            break;
        default:
            errors->createNewError(GENERIC, expr.lnk, "cannot get integer value from non integer expression");
            break;
    }
}

Expression RuntimeEngine::parseArrayExpression(Expression& interm, Ast* pAst) {
    Expression expression(interm), indexExpr;

    indexExpr = parseExpression(pAst);

    expression.newExpression=false;
    expression.utype.array = false;
    expression.utype.array = false;
    expression.arrayElement=true;
    expression.func=false;
    expression.link = pAst;
    bool referenceAffected = currentRefrenceAffected(indexExpr);

    switch(interm.type) {
        case expression_field:
            if(!interm.utype.field->isArray) {
                // error not an array
                errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));

            if(interm.utype.field->type == CLASS) {
                expression.utype.klass = interm.utype.field->klass;
                expression.type = expression_lclass;

                expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            } else if(interm.utype.field->type == VAR) {
                expression.type = expression_var;
                expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            } else if(interm.utype.field->type == OBJECT) {
                expression.type = expression_objectclass;
                expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            }else {
                expression.type = expression_unknown;
            }
            break;
        case expression_string:
            errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            break;
        case expression_var:
            if(!interm.arrayObject()) {
                // error not an array
                errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            if(!interm.func) {
                expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }

            pushExpressionToRegister(indexExpr, expression, ebx);


            if(interm.func) {
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            } else {
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            }


            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, ebx));

            if(interm.func) {
                expression.code.push_i64(SET_Ei(i64, op_POP));
            } else {
                // do nothing??? no idea man
            }
            break;
        case expression_lclass:
            if(!interm.utype.array) {
                // error not an array
                errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));

            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            break;
        case expression_null:
            errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "null cannot be used as an array");
            break;
        case expression_objectclass:
            if(!interm.utype.array) {
                // error not an array
                errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            if(c_options.optimize) {
                if(referenceAffected)
                    expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            } else
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));

            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));

            break;
        case expression_void:
            errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "void cannot be used as an array");
            break;
        case expression_unresolved:
            /* do nothing */
            break;
        default:
            errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "invalid array expression before `[`");
            break;
    }

    if(indexExpr.type == expression_var) {
        // we have an integer!
    } else if(indexExpr.type == expression_field) {
        if(!indexExpr.utype.field->isVar()) {
            errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "array index is not an integer");
        }
    } else if(indexExpr.type == expression_unresolved) {
    } else {
        errors->createNewError(GENERIC, indexExpr.lnk->line, indexExpr.lnk->col, "array index is not an integer");
    }

    return expression;
}


Expression &RuntimeEngine::parseDotNotationChain(Ast *pAst, Expression &expression, unsigned int startpos) {

    Ast* utype;
    Expression rightExpr;
    rightExpr =expression;
    for(unsigned int i = startpos; i < pAst->getSubAstCount(); i++) {
        utype = pAst->getSubAst(i);

        if(rightExpr.utype.array && utype->gettype() != ast_expression) {
            errors->createNewError(GENERIC, utype->line, utype->col, "expected array index `[]`");
        }

        if(utype->gettype() == ast_dotnotation_call_expr) {
            rightExpr = parseDotNotationChain(utype, rightExpr, 0);
        }
        else if(utype->gettype() == ast_expression){ /* array expression */
            rightExpr = parseArrayExpression(rightExpr, utype);
        }
        else {
            rightExpr = parseDotNotationCallContext(rightExpr, utype);
        }

        rightExpr.link = utype;
        if(rightExpr.type == expression_unresolved || rightExpr.type == expression_unknown)
            break;

        expression.code.inject(expression.code.size(), rightExpr.code);
        expression.type = rightExpr.type;
        expression.utype = rightExpr.utype;
        expression.utype.array = rightExpr.utype.array;
        expression.func=rightExpr.func;
        rightExpr.code.free();
    }
    return expression;
}

Expression RuntimeEngine::parseDotNotationCall(Ast* pAst) {
    string method_name="";
    Expression expression, interm;
    Method* fn;
    Ast* pAst2;

    if(pAst->hassubast(ast_dot_fn_e)) {
        pAst2 = pAst->getSubAst(ast_dot_fn_e);
        fn = resolveMethodUtype(pAst2, pAst2, expression);
        if(fn != NULL) {

            expression.type = methodReturntypeToExpressionType(fn);
            if(expression.type == expression_lclass) {
                expression.utype.klass = fn->klass;
                expression.utype.type = CLASS;
            }

            expression.func = true;
            expression.utype.array = fn->array;

            if(pAst->hasentity(_INC) || pAst->hasentity(_DEC)) {
                token_entity entity = pAst->hasentity(_INC) ? pAst->getentity(_INC) : pAst->getentity(_DEC);
                OperatorOverload* overload;
                List<Param> emptyParams;

                expression.type = expression_var;
                if(fn->array) {
                    errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "call to function `" + fn->getName() + paramsToString(*fn->getParams()) + "` must return an var to use `" + entity.gettoken() + "` operator");
                } else {
                    switch(fn->type) {
                        case TYPEVOID:
                            errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "cannot use `" + entity.gettoken() + "` operator on function that returns void ");
                            break;
                        case VAR:
                            if(pAst->hasentity(_INC))
                                expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                            else
                                expression.code.push_i64(SET_Di(i64, op_DEC, ebx));
                            break;
                        case OBJECT:
                            errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "function returning object must be casted before using `"
                                                                                            + entity.gettoken() + "` operator");
                            break;
                        case CLASS:
                            if((overload = fn->klass->getOverload(stringToOp(entity.gettoken()), emptyParams)) != NULL) {
                                // add code to call overload
                                expression.type = methodReturntypeToExpressionType(overload);
                                if(expression.type == expression_lclass) {
                                    expression.utype.klass = overload->klass;
                                    expression.utype.type = CLASS;
                                }

                                expression.code.push_i64(SET_Di(i64, op_CALL, overload->address));
                            } else if(fn->klass->hasOverload(stringToOp(entity.gettoken()))) {
                                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "call to function `" + fn->getName() + paramsToString(*fn->getParams()) + "`; missing overload params for operator `"
                                                                                                + fn->klass->getFullName() + ".operator" + entity.gettoken() + "`");
                            } else {
                                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "call to function `" + fn->getName() + paramsToString(*fn->getParams()) + "` must return an int to use `" + entity.gettoken() + "` operator");
                            }
                            break;
                        case UNDEFINED:
                            // do nothing
                            break;
                    }
                }
            }
        } else
            expression.type = expression_unresolved;
    } else {
        expression = parseUtype(pAst);
    }

    if(pAst->getSubAstCount() > 1) {
        // chained calls
        if(expression.type == expression_void) {
            errors->createNewError(GENERIC, pAst->getSubAst(1)->line, pAst->getSubAst(1)->col, "illegal access to function of return type `void`");
        } else
            parseDotNotationChain(pAst, expression, 1);
    }

    if(pAst->hasentity(DOT)) {
        expression.dot = true;
    }

    expression.lnk = pAst;
    return expression;
}

Expression RuntimeEngine::parsePrimaryExpression(Ast* ast) {
    ast = ast->getSubAst(0);

    switch(ast->gettype()) {
        case ast_literal_e:
            return parseLiteral(ast->getSubAst(ast_literal));
        case ast_utype_class_e:
            return psrseUtypeClass(ast);
        case ast_dot_not_e:
            return parseDotNotationCall(ast->getSubAst(ast_dotnotation_call_expr));
        default:
            stringstream err;
            err << ": unknown ast type: " << ast->gettype();
            errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
            return Expression(ast); // not an expression!
    }
}

Method* RuntimeEngine::resolveSelfMethodUtype(Ast* utype, Ast* valueList, Expression& out) {
    Scope* scope = currentScope();
    Method* fn = NULL;
    utype = valueList->getSubAst(ast_utype);
    valueList = valueList->getSubAst(ast_value_list);

    /* This is a naked utype so we dont haave to worry about brackets */
    ReferencePointer ptr;
    string methodName = "";
    List<Param> params;
    List<Expression> expressions = parseValueList(valueList);
    Expression expression;

    expressionListToParams(params, expressions);
    ptr = parseTypeIdentifier(utype);

    if(splitMethodUtype(methodName, ptr)) {
        // accessor
        if(scope->type == GLOBAL_SCOPE) {

            errors->createNewError(GENERIC, valueList->line, valueList->col,
                             "cannot get object `" + methodName + paramsToString(params) + "` from self at global scope");
        }

        scope->self = true;
        resolveUtype(ptr, expression, utype);
        scope->self = false;
        if(expression.type == expression_class || (expression.type == expression_field && expression.utype.field->type == CLASS)) {
            ClassObject* klass;
            if(expression.type == expression_class) {
                klass = expression.utype.klass;
            } else {
                klass = expression.utype.field->klass;
            }

            if((fn = klass->getFunction(methodName, params, true)) != NULL){}
            else if((fn = klass->getOverload(stringToOp(methodName), params, true)) != NULL){}
            else if(methodName == klass->getName() && (fn = klass->getConstructor(params)) != NULL) {}
            else {
                if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + methodName;

                errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + methodName + paramsToString(params) + "`");
            }
        } else if(expression.utype.type == UNDEFINED) {
        }
        else {
            errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + methodName + paramsToString(params) +  "`");
        }
    } else {
        // method or macros
        if(ptr.singleRefrence()) {
            if(scope->type == GLOBAL_SCOPE) {
                // must be macros
                errors->createNewError(GENERIC, valueList->line, valueList->col,
                                 "cannot get object `" + ptr.referenceName + paramsToString(params) + "` from self at global scope");
            } else {

                if((fn = scope->klass->getFunction(ptr.referenceName, params, true)) != NULL){}
                else if((fn = scope->klass->getOverload(stringToOp(ptr.referenceName), params, true)) != NULL){}
                else if(ptr.referenceName == scope->klass->getName() && (fn = scope->klass->getConstructor(params)) != NULL) {}
                else {
                    if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + ptr.referenceName;
                    else methodName = ptr.referenceName;

                    errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + ptr.referenceName +  paramsToString(params) + "`");
                }
            }
        } else {
            // must be macros
            errors->createNewError(GENERIC, valueList->line, valueList->col, " use of module {" + ptr.referenceName + "} in expression signifies global access of object");

        }
    }

    if(fn != NULL) {
        if(!fn->isStatic() && scope->type == STATIC_BLOCK) {
            errors->createNewError(GENERIC, utype->line, utype->col, "call to instance function in static context");
        }

        if(!fn->isStatic())
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

        for(unsigned int i = 0; i < expressions.size(); i++) {
            pushExpressionToStack(expressions.get(i), out);
        }
        out.code.push_i64(SET_Di(i64, op_CALL, fn->address));
    }

    freeList(params);
    freeList(expressions);
    ptr.free();
    return fn;
}

Expression RuntimeEngine::parseSelfDotNotationCall(Ast* pAst) {
    Scope* scope = currentScope();
    string method_name="";
    Expression expression;
    Method* fn;
    Ast* pAst2;

    if(pAst->hassubast(ast_dot_fn_e)) {
        pAst2 = pAst->getSubAst(ast_dot_fn_e);
        fn = resolveSelfMethodUtype(pAst2, pAst2, expression);
        if(fn != NULL) {
            expression.type = methodReturntypeToExpressionType(fn);
            if(expression.type == expression_lclass) {
                expression.utype.klass = fn->klass;
                expression.utype.type = CLASS;
            }

            expression.utype.array = fn->array;
        } else
            expression.type = expression_unresolved;

    } else {
        scope->self = true;
        expression = parseUtype(pAst);
        scope->self = false;
    }

    if(pAst->hasentity(DOT)) {
        expression.dot = true;
    }


    if(pAst->getSubAstCount() > 1) {
        // chained calls
        parseDotNotationChain(pAst, expression, 1);
    }

    expression.lnk = pAst;
    return expression;
}

Expression RuntimeEngine::parseSelfExpression(Ast* pAst) {
    Scope* scope = currentScope();
    Expression expression;

    if(pAst->hasentity(PTR)) {
        // self-><expression>
        return parseSelfDotNotationCall(pAst->getSubAst(ast_dotnotation_call_expr));
    } else {
        // self
        expression.type = expression_lclass;
        expression.utype.type = CLASS;
        expression.utype.klass = scope->klass;
        expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
    }

    if(scope->type == GLOBAL_SCOPE || scope->type == STATIC_BLOCK) {
        errors->createNewError(GENERIC, pAst->line, pAst->col, "illegal reference to self in static context");
        expression.type = expression_unknown;
    }

    expression.lnk = pAst;
    return expression;
}

Method* RuntimeEngine::resolveBaseMethodUtype(Ast* utype, Ast* valueList, Expression& out) {
    Scope* scope = currentScope();
    Method* fn = NULL;
    utype = valueList->getSubAst(ast_utype);
    valueList = valueList->getSubAst(ast_value_list);

    /* This is a naked utype so we dont haave to worry about brackets */
    ReferencePointer ptr;
    string methodName = "";
    List<Param> params;
    List<Expression> expressions = parseValueList(valueList);
    Expression expression;

    expressionListToParams(params, expressions);
    ptr = parseTypeIdentifier(utype);
    
    if(splitMethodUtype(methodName, ptr)) {
        if(scope->type == GLOBAL_SCOPE) {
            errors->createNewError(GENERIC, valueList->line, valueList->col,
                             "cannot get object `" + methodName + paramsToString(params) + "` from self at global scope");
        }

        scope->base = true;
        resolveUtype(ptr, expression, utype);
        scope->base = false;
        if(expression.type == expression_class || (expression.type == expression_field && expression.utype.field->type == CLASS)) {
            ClassObject* klass;
            if(expression.type == expression_class) {
                klass = expression.utype.klass;
            } else {
                klass = expression.utype.field->klass;
            }

            if((fn = klass->getFunction(methodName, params)) != NULL){}
            else if((fn = klass->getOverload(stringToOp(methodName), params)) != NULL){}
            else if(methodName == klass->getName() && (fn = klass->getConstructor(params)) != NULL) {}
            else {
                if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + methodName;

                errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + (klass == NULL ? ptr.referenceName : methodName) + paramsToString(params) + "`");
            }
        } else if(expression.utype.type == UNDEFINED) {
        }
        else {
            errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + (expression.utype.klass == NULL ? ptr.referenceName : methodName) + paramsToString(params) +  "`");
        }
    } else {
        // method or macros
        if(ptr.singleRefrence()) {
            if(scope->type == GLOBAL_SCOPE) {
                // must be macros
                errors->createNewError(GENERIC, valueList->line, valueList->col,
                                 "cannot get object `" + ptr.referenceName + paramsToString(params) + "` from self at global scope");
            } else {
                ClassObject* base, *start = scope->klass->getBaseClass();

                if(start == NULL) {
                    errors->createNewError(GENERIC, utype->getSubAst(ast_type_identifier)->line, utype->getSubAst(ast_type_identifier)->col, "class `" + scope->klass->getFullName() + "` does not inherit a base class");
                    return NULL;
                }

                for(;;) {
                    base = start;

                    if(base == NULL) {
                        // could not resolve
                        if(stringToOp(methodName) != oper_UNDEFINED) methodName = "operator" + ptr.referenceName;
                        else methodName = ptr.referenceName;

                        errors->createNewError(COULD_NOT_RESOLVE, valueList->line, valueList->col, " `" + ptr.referenceName 
                                                                                             +  paramsToString(params) + "`");
                        return NULL;
                    }

                    if((fn = base->getFunction(ptr.referenceName, params)) != NULL){ break; }
                    else if((fn = base->getOverload(stringToOp(ptr.referenceName), params)) != NULL){ break; }
                    else if(ptr.referenceName == base->getName() && (fn = base->getConstructor(params)) != NULL) { break; }
                    else {
                        start = base->getBaseClass(); // recursivley assign klass to new base
                    }
                }
            }
        } else {
            // must be macros
            errors->createNewError(GENERIC, valueList->line, valueList->col, " use of module {" + ptr.referenceName + "} in expression signifies global access of object");

        }
    }


    if(fn != NULL) {
        if(!fn->isStatic() && scope->type == STATIC_BLOCK) {
            errors->createNewError(GENERIC, utype->line, utype->col, "call to instance function in static context");
        }

        if(!fn->isStatic())
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

        for(unsigned int i = 0; i < expressions.size(); i++) {
            pushExpressionToStack(expressions.get(i), out);
        }
        out.code.push_i64(SET_Di(i64, op_CALL, fn->address));
    }

    freeList(params);
    freeList(expressions);
    ptr.free();
    return fn;
}

Expression RuntimeEngine::parseBaseDotNotationCall(Ast* pAst) {
    Scope* scope = currentScope();
    pAst = pAst->getSubAst(ast_dotnotation_call_expr);
    Expression expression;
    string method_name="";
    Method* fn;
    Ast* pAst2;

    if(pAst->hassubast(ast_dot_fn_e)) {
        pAst2 = pAst->getSubAst(ast_dot_fn_e);
        fn = resolveBaseMethodUtype(pAst2->getSubAst(ast_utype), pAst2->getSubAst(ast_value_list), expression);
        if(fn != NULL) {
            expression.type = methodReturntypeToExpressionType(fn);
            if(expression.type == expression_lclass) {
                expression.utype.klass = fn->klass;
                expression.utype.type = CLASS;
            }

            expression.utype.array = fn->array;
        } else
            expression.type = expression_unresolved;

    } else {
        scope->base = true;
        expression = parseUtype(pAst->getSubAst(ast_utype));
        scope->base = false;
    }

    if(pAst->hasentity(DOT)) {
        expression.dot = true;
    }

    if(pAst->getSubAstCount() > 1) {
        // chained calls
        parseDotNotationChain(pAst, expression, 1);
    }

    expression.lnk = pAst;
    return expression;
}

Expression RuntimeEngine::parseBaseExpression(Ast* pAst) {
    Scope* scope = currentScope();
    Expression expression;

    expression = parseBaseDotNotationCall(pAst);

    expression.lnk = pAst;
    return expression;
}

Expression RuntimeEngine::parseNullExpression(Ast* pAst) {
    Expression expression(pAst);

    expression.utype.referenceName = "null";
    expression.type = expression_null;
    return expression;
}

List<Expression> RuntimeEngine::parseVectorArray(Ast* pAst) {
    List<Expression> vecArry;
    pAst =

    for(unsigned int i = 0; i < pAst->getSubAstCount(); i++) {
        vecArry.add(parseExpression(pAst->getSubAst(i)));
    }

    return vecArry;
}

void RuntimeEngine::checkVectorArray(Expression& utype, List<Expression>& vecArry) {
    if(utype.utype.type == UNDEFINED) return;

    for(unsigned int i = 0; i < vecArry.size(); i++) {
        if(vecArry.get(i).type != expression_unresolved) {
            switch(vecArry.get(i).type) {
                case expression_native:
                    errors->createNewError(UNEXPECTED_SYMBOL, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, " `" + ResolvedReference::typeToString(utype.utype.type) + "`");
                    break;
                case expression_lclass:
                    if(utype.utype.type == CLASS) {
                        if(!utype.utype.klass->match(vecArry.get(i).utype.klass)){
                            errors->createNewError(INCOMPATIBLE_TYPES, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, ": initalization of class `" +
                                                                                                                    utype.utype.klass->getName() + "` is not compatible with class `" + vecArry.get(i).utype.klass->getName() + "`");
                        }
                    } else {
                        errors->createNewError(INCOMPATIBLE_TYPES, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, ": type `" + utype.utype.typeToString() + "` is not compatible with type `"
                                                                                                                + vecArry.get(i).utype.typeToString() + "`");
                    }
                    break;
                case expression_var:
                    if(utype.utype.type != VAR) {
                        errors->createNewError(INCOMPATIBLE_TYPES, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, ": type `" + utype.utype.typeToString() + "` is not compatible with type `"
                                                                                                                + vecArry.get(i).utype.typeToString() + "`");
                    }else {
                        if(utype.utype.isVar() && vecArry.get(i).utype.isVar()) {}
                        else if(utype.utype.dynamicObject() && vecArry.get(i).utype.dynamicObject()) {}
                        else {
                            errors->createNewError(INCOMPATIBLE_TYPES, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, ": type `" + utype.utype.typeToString() + "` is not compatible with type `"
                                                                                                                    + vecArry.get(i).utype.typeToString() + "`");
                        }
                    }
                    break;
                case expression_null:
                    if(utype.utype.type != CLASS) {
                        errors->createNewError(GENERIC, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, "cannot assign null to type `" + utype.utype.typeToString() + "1");
                    }
                    break;
                case expression_string:
                    errors->createNewError(GENERIC, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, "multi dimentional array are not supported yet, use string() class instead");
                    break;
                case expression_objectclass:
                    if(utype.utype.type != CLASS) {
                        errors->createNewError(GENERIC, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, "cannot assign null to type `" + utype.utype.typeToString() + "`");
                    }
                    break;
                default:
                    errors->createNewError(GENERIC, vecArry.get(i).lnk->line, vecArry.get(i).lnk->col, "error processing vector array");
                    break;
            }
        }
    }
}

void RuntimeEngine::parseNewArrayExpression(Expression& out, Expression& utype, Ast* pAst) {
    Expression sizeExpr = parseExpression(pAst);

    pushExpressionToRegister(sizeExpr, out, ebx);

    out.code.push_i64(SET_Di(i64, op_INC, sp));
    out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
    if(out.type == expression_var)
        out.code.push_i64(SET_Di(i64, op_NEWARRAY, ebx));
    else if(out.type == expression_lclass) {
        out.code.push_i64(SET_Ci(i64, op_NEWCLASSARRAY, ebx, 0, utype.utype.klass->address));
    }
    else
        out.code.push_i64(SET_Di(i64, op_NEWOBJARRAY, ebx));
}

Expression RuntimeEngine::parseNewExpression(Ast* pAst) {
    Scope* scope = currentScope();
    Expression expression, utype;
    List<Expression> expressions;
    List<Param> params;

    utype = parseUtype(pAst->getSubAst(ast_utype));
    if(pAst->hassubast(ast_vector_array)) {
        // vec array
        expressions = parseVectorArray(pAst);
        checkVectorArray(utype, expressions);

        expression.type = utype.type;
        expression.utype = utype.utype;
        if(expression.type == expression_class) {
            expression.type = expression_lclass;
        } else if(expression.type == expression_native) {
            if(utype.utype.isVar())
                expression.type = expression_var;
            else if(utype.utype.dynamicObject())
                expression.type = expression_objectclass;
        }
        expression.utype.array = true;
    }
    else if(pAst->hassubast(ast_array_expression)) {
        expression.type = utype.type;
        expression.utype = utype.utype;
        if(expression.type == expression_class) {
            expression.type = expression_lclass;
        } else if(expression.type == expression_native) {
            if(utype.utype.isVar())
                expression.type = expression_var;
            else if(utype.utype.dynamicObject())
                expression.type = expression_objectclass;
        }
        expression.utype.array = true;
        parseNewArrayExpression(expression, utype, pAst->getSubAst(ast_array_expression));
    }
    else {
        // ast_value_list
        if(pAst->hassubast(ast_value_list))
            expressions = parseValueList(pAst->getSubAst(ast_value_list));
        if(utype.type == expression_class) {
            Method* fn=NULL;
            if(!pAst->hassubast(ast_value_list)) {
                errors->createNewError(GENERIC, utype.lnk->line, utype.lnk->col, "expected '()' after class " + utype.utype.klass->getName());
            } else {
                expressionListToParams(params, expressions);
                if((fn=utype.utype.klass->getConstructor(params))==NULL) {
                    errors->createNewError(GENERIC, utype.lnk->line, utype.lnk->col, "class `" + utype.utype.klass->getFullName() +
                                                                               "` does not contain constructor `" + utype.utype.klass->getName() + paramsToString(params) + "`");
                }
            }
            expression.type = expression_lclass;
            expression.utype = utype.utype;

            if(fn!= NULL) {

                expression.code.push_i64(SET_Di(i64, op_INC, sp));
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
                expression.code.push_i64(SET_Di(i64, op_NEWCLASS, utype.utype.klass->address));

                for(unsigned int i = 0; i < expressions.size(); i++) {
                    pushExpressionToStack(expressions.get(i), expression);
                }
                expression.code.push_i64(SET_Di(i64, op_CALL, fn->address));
            }
        } else if(utype.type == expression_native) {
            // native creation
            if(pAst->hassubast(ast_value_list)) {
                errors->createNewError(GENERIC, utype.lnk->line, utype.lnk->col, " native type `" + utype.utype.typeToString() + "` does not contain a constructor");
            }
            expression.type = expression_native;
            expression.utype = utype.utype;
        } else {
            if(utype.utype.type != UNDEFINED)
                errors->createNewError(COULD_NOT_RESOLVE, utype.lnk->line, utype.lnk->col, " `" + utype.utype.typeToString() + "`");
        }
    }

    freeList(params);
    freeList(expressions);
    expression.newExpression = true;
    expression.link = pAst;
    return expression;
}

Expression RuntimeEngine::parseSizeOfExpression(Ast* pAst) {
    Expression expression = parseExpression(pAst->getSubAst(ast_expression)), out;
    switch(expression.type) {
        case expression_void:
        case expression_unknown:
        case expression_class:
        case expression_native:
        case expression_null:
            errors->createNewError(GENERIC, expression.lnk, "cannot get sizeof from expression of type `" + expression.typeToString() + "`");
            break;
        case expression_string:
            out.code.push_i64(SET_Di(i64, op_MOVI, expression.value.size()), ebx); // This is silly but we will allow it
            break;
        default:
            if(expression.type == expression_var && expression.literal) {
                errors->createNewError(GENERIC, expression.lnk, "cannot get sizeof from literal value");
            }

            pushExpressionToPtr(expression, out);

            if(out.code.size() == 0)
                out.code.push_i64(SET_Di(i64, op_MOVI, 1), ebx); // just in case out object isnt an object
            else {
                if(expression.func) {
                    out.code.push_i64(SET_Di(i64, op_SIZEOF, ebx)); // just in case out object isnt an object
                    out.code.push_i64(SET_Ei(i64, op_POP));
                } else
                    out.code.push_i64(SET_Di(i64, op_SIZEOF, ebx)); // just in case out object isnt an object
            }
            break;
    }

    out.type = expression_var;
    return out;
}

Expression RuntimeEngine::parseIntermExpression(Ast* pAst) {
    Expression expression;

    switch(pAst->gettype()) {
        case ast_primary_expr:
            return parsePrimaryExpression(pAst);
        case ast_self_e:
            return parseSelfExpression(pAst);
        case ast_base_e:
            return parseBaseExpression(pAst);
        case ast_null_e:
            return parseNullExpression(pAst);
        case ast_new_e:
            return parseNewExpression(pAst);
        case ast_sizeof_e:
            return parseSizeOfExpression(pAst);
        case ast_post_inc_e:
            return parsePostInc(pAst);
        case ast_arry_e:
            return parseArrayExpression(pAst);
        case ast_cast_e:
            return parseCastExpression(pAst);
        case ast_pre_inc_e:
            return parsePreInc(pAst);
        case ast_paren_e:
            return parseParenExpression(pAst);
        case ast_not_e:
            return parseNotExpression(pAst);
        case ast_vect_e:
            errors->createNewError(GENERIC, pAst->line, pAst->col, "unexpected vector array expression. Did you mean 'new type { <data>, .. }'?");
            expression.lnk = pAst;
            return expression;
        case ast_add_e:
        case ast_mult_e:
        case ast_shift_e:
        case ast_less_e:
        case ast_equal_e:
        case ast_and_e:
            return parseBinaryExpression(pAst);
        case ast_ques_e:
            return parseQuesExpression(pAst);
        default:
            errors->createNewError(GENERIC, pAst->line, pAst->col, "unexpected expression format");
            return Expression(pAst); // not an expression!
    }
}

void RuntimeEngine::postIncClass(Expression& out, token_entity op, ClassObject* klass) {
    OperatorOverload* overload;
    List<Param> emptyParams;

    if(op.gettokentype() == _INC) {
        overload = klass->getPostIncOverload();
    } else {
        overload = klass->getPostDecOverload();
    }

    if(overload != NULL) {
        // add code to call overload

        out.code.push_i64(SET_Di(i64, op_MOVI, 1), ebx);
        out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));

        if(!overload->isStatic())
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

        out.code.push_i64(SET_Di(i64, op_CALL, overload->address));

        out.type = methodReturntypeToExpressionType(overload);
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
        } else {

        }
    } else if(klass->hasOverload(stringToOp(op.gettoken()))) {
        errors->createNewError(GENERIC, op.getline(), op.getcolumn(), "use of class `" + klass->getName() + "`; missing overload params for operator `"
                                                                + klass->getFullName() + ".operator" + op.gettoken() + "`");
        out.type = expression_var;
    } else {
        errors->createNewError(GENERIC, op.getline(), op.getcolumn(), "use of class `" + klass->getName() + "` must return an int to use `" + op.gettoken() + "` operator");
        out.type = expression_var;
    }
}

Expression RuntimeEngine::parsePostInc(Ast* pAst) {
    Expression interm;
    token_entity entity = pAst->hasentity(_INC) ? pAst->getentity(_INC) : pAst->getentity(_DEC);

    interm = parseIntermExpression(pAst->getSubAst(0));
    Expression expression(interm);

    if(interm.utype.array){
        errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "expression must evaluate to an int to use `" + entity.gettoken() + "` operator");
    } else {
        switch(interm.type) {
            case expression_var:
                if(interm.func) {
                    pushExpressionToRegister(interm, expression, ebx);
                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                    else
                        expression.code.push_i64(SET_Di(i64, op_DEC, ebx));
                } else {
                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                    else
                        expression.code.push_i64(SET_Di(i64, op_DEC, ebx));
                }
                break;
            case expression_field:
                if(interm.utype.field->type == CLASS) {
                    if(expression.utype.field->local)
                        expression.code.push_i64(SET_Di(i64, op_MOVL, expression.utype.field->address));
                    postIncClass(expression, entity, interm.utype.field->klass);
                    return expression;
                } else if(interm.utype.field->type == VAR || interm.utype.field->type == OBJECT) {
                    if(interm.utype.field->type == OBJECT) {
                        errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "use of `" + entity.gettoken() + "` operator on field of type `dynamic_object` without a cast. Try ((SomeClass)dynamic_class)++");
                    } else if(interm.utype.field->isVar()) {
                        // increment the field
                        pushExpressionToRegisterNoInject(interm, expression, ebx);
                        expression.code.push_i64(SET_Di(i64, op_MOVI, 1), ecx);

                        if(entity.gettokentype() == _INC)
                            expression.code.push_i64(
                                    SET_Ci(i64, op_ADDL, ecx,0 , interm.utype.field->address));
                        else
                            expression.code.push_i64(
                                    SET_Ci(i64, op_SUBL, ecx,0 , interm.utype.field->address));
                    }
                } else if(interm.utype.field->type != UNDEFINED){
                    // do nothing field is unresolved
                    errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "expression must evaluate to an int to use `" + entity.gettoken() + "` operator");

                }
                break;
            case expression_lclass:
                if(interm.func)
                    expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
                postIncClass(expression, entity, interm.utype.klass);
                return expression;
                break;
            case expression_objectclass:
                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "use of `" + entity.gettoken() + "` operator on type `object` without a cast. Try ((SomeClass)object)++");
                break;
            case expression_null:
                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "value `null` cannot be used as var");
                break;
            case expression_native:
                errors->createNewError(UNEXPECTED_SYMBOL, entity.getline(), entity.getcolumn(), " `" + interm.utype.typeToString() + "`");
                break;
            case expression_string:
                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "increment on immutable string");
                break;
            case expression_unresolved:
                // do nothing
                break;
            default:
                errors->createNewError(GENERIC, entity.getline(), entity.getcolumn(), "expression must evaluate to an int to use `" + entity.gettoken() + "` operator");
                break;
        }
    }

    expression.type = expression_var;
    return expression;
}

Expression RuntimeEngine::parseArrayExpression(Ast* pAst) {
    Expression expression, interm, indexExpr, rightExpr;
    Field* field;

    interm = parseIntermExpression(pAst->getSubAst(0));
    indexExpr = parseExpression(pAst->getSubAst(1));

    if(!interm.newExpression)
        expression.code.inject(expression.code.__asm64.size(), interm.code);
    expression.type = interm.type;
    expression.utype = interm.utype;
    expression.utype.array = false;
    expression.newExpression=false;
    expression.arrayElement=true;
    expression.func=false;
    expression.link = pAst;
    bool referenceAffected = currentRefrenceAffected(indexExpr);

    switch(interm.type) {
        case expression_field:
            if(!interm.utype.field->isArray) {
                // error not an array
                errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));


            if(interm.utype.field->type == CLASS) {
                expression.utype.klass = interm.utype.field->klass;
                expression.type = expression_lclass;

                expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            } else if(interm.utype.field->type == VAR) {
                expression.type = expression_var;
                expression.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, ebx));
            }
            else if(interm.utype.field->type == OBJECT) {
                expression.type = expression_objectclass;
                expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            } else {
                expression.type = expression_unknown;
            }
            
            break;
        case expression_string:
            errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            break;
        case expression_var:
            if(!interm.utype.array) {
                // error not an array
                errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));

            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Ci(i64, op_IALOAD_2, ebx,0, ebx));

            break;
        case expression_lclass:
            if(!interm.utype.array) {
                // error not an array
                errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            if(interm.newExpression) {
                expression.code.inject(expression.code.__asm64.size(), interm.code);
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            }

            if(!interm.newExpression) {
                expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
            }


            pushExpressionToRegister(indexExpr, expression, ebx);

            if(!interm.newExpression) {
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            }

            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));

            if(interm.newExpression) {
                expression.code.push_i64(SET_Ei(i64, op_POP));
            }
            break;
        case expression_null:
            errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "null cannot be used as an array");
            break;
        case expression_objectclass:
            if(!interm.utype.array) {
                // error not an array
                errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "expression of type `" + interm.typeToString() + "` must evaluate to array");
            }

            expression.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            pushExpressionToRegister(indexExpr, expression, ebx);

            expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            expression.code.push_i64(SET_Di(i64, op_CHECKLEN, ebx));
            expression.code.push_i64(SET_Di(i64, op_MOVND, ebx));
            break;
        case expression_void:
            errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "void cannot be used as an array");
            break;
        case expression_unresolved:
            /* do nothing */
            break;
        default:
            errors->createNewError(GENERIC, pAst->getSubAst(0)->line, pAst->getSubAst(0)->col, "invalid array expression before `[`");
            break;
    }

    if(indexExpr.type == expression_var) {
        // we have an integer!
    } else if(indexExpr.type == expression_field) {
        if(!indexExpr.utype.field->isVar()) {
            errors->createNewError(GENERIC, pAst->getSubAst(1)->line, pAst->getSubAst(1)->col, "array index is not an integer");
        }
    } else if(indexExpr.type == expression_unresolved) {
    } else {
        errors->createNewError(GENERIC, pAst->getSubAst(1)->line, pAst->getSubAst(1)->col, "array index is not an integer");
    }

    if(pAst->getSubAstCount() > 2) {
        // expression after (we may not need this?)
        parseDotNotationChain(pAst->getSubAst(2), expression, 0);
    }

    return expression;
}

void RuntimeEngine::parseClassCast(Expression& utype, Expression& arg, Expression& out) {
    if(arg.type != expression_unresolved) {
        if(arg.utype.array != utype.utype.array) {
            errors->newerror(INCOMPATIBLE_TYPES, utype.lnk->line, utype.lnk->col, "; cannot cast `" +
                    arg.utype.typeToString() + "` to `" + utype.utype.typeToString() + "`");
            out.type = expression_unresolved;
            return;
        }
    }

    if(utype.type == expression_class)
        utype.type = expression_lclass;

    switch(arg.type) {
        case expression_lclass:
            if(utype.utype.klass->hasBaseClass(arg.utype.klass)) {
                out.inject(arg);
                out.utype = utype.utype;
                out.type = expression_lclass;
                return;
            }
            break;
        case expression_objectclass:
            // TODO: put runtime code to evaluate at runtime
            pushExpressionToPtr(arg, out);
            out.code.push_i64(SET_Di(i64, op_CAST, utype.utype.klass->address));
            out.type = utype.type;
            out.utype = utype.utype;
            return;
        case expression_field:
            if(arg.utype.field->type == CLASS) {
                if(utype.utype.klass->match(arg.utype.klass)) {
                    out.inject(arg);
                    out.type = expression_lclass;
                    out.utype = utype.utype;
                    return;
                }
            } else if(arg.utype.field->type == OBJECT) {
                pushExpressionToPtr(arg, out);
                out.code.push_i64(SET_Di(i64, op_CAST, utype.utype.klass->address));
                out.type = expression_lclass;
                out.utype = utype.utype;
                return;
            } else if(arg.utype.field->type == VAR) {
                errors->newerror(GENERIC, utype.lnk->line, utype.lnk->col, "field `" + arg.utype.field->name + "` is not a class; "
                        "cannot cast `" + arg.utype.typeToString() + "` to `" + utype.utype.typeToString() + "`");
                return;
            } else {
                // field is unresolved
                out.type = expression_unresolved;
                out.utype = utype.utype;
                return;
            }
            break;
    }

    if(arg.type != expression_unresolved)
        errors->newerror(INCOMPATIBLE_TYPES, utype.lnk->line, utype.lnk->col, "; cannot cast `" + arg.utype.typeToString() + "` to `" + utype.utype.typeToString() + "`");
    return;
}

// TODO: add native support for casting
void RuntimeEngine::parseNativeCast(Expression& utype, Expression& arg, Expression& out) {
    Scope* scope = currentScope();
    if(arg.type != expression_unresolved) {
        if(arg.utype.array != utype.utype.array) {
            errors->newerror(INCOMPATIBLE_TYPES, utype.lnk->line, utype.lnk->col, "; cannot cast `" + arg.typeToString() + "` to `" + utype.typeToString() + "`");
            out.type = expression_unresolved;
            return;
        }
    }

    if(arg.utype.type != CLASSFIELD && utype.utype.type == arg.utype.type)
        createNewWarning(GENERIC, utype.lnk->line, utype.lnk->col, "redundant cast of type `" + arg.typeToString() + "` to `" + utype.typeToString() + "`");
    else if(arg.utype.type == CLASSFIELD && utype.utype.type == CLASSFIELD)
        createNewWarning(GENERIC, utype.lnk->line, utype.lnk->col, "redundant cast of type `" + arg.typeToString() + "` to `" + utype.typeToString() + "`");

    out.type = expression_var;
    out.utype = utype.utype;
    switch(utype.utype.type) {
//        case fi8:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOV8, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOV8, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fi16:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOV16, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOV16, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fi32:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOV32, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOV32, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fi64:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOV64, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOV64, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fui8:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOVU8, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOVU8, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fui16:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOVU16, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOVU16, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fui32:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOVU32, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOVU32, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
//        case fui64:
//            if(arg.type == expression_var) {
//                out.code.push_i64(SET_Ci(i64, op_MOVU64, ebx,0, ebx));
//                return;
//            } else if(arg.type == expression_field) {
//                pushExpressionToRegisterNoInject(arg, out, ebx);
//                out.code.push_i64(SET_Ci(i64, op_MOVU64, ebx, 0, ebx));
//                out.code.push_i64(SET_Ci(i64, op_MOVR, adx, 0, fp));
//                out.code.push_i64(SET_Ci(i64, op_SMOVR, ebx, 0, arg.utype.field->vaddr));
//                return;
//            }
//            break;
        case VAR:
            if(arg.type == expression_var || arg.type==expression_field) {
                return;
            } // TODO: do we even have to do anything here?
            break;
        case TYPEVOID:
            errors->newerror(GENERIC, utype.lnk->line, utype.lnk->col, "type `void` cannot be used as a cast");
            return;
        case OBJECT:
            out.type = expression_objectclass;
            if(arg.type == expression_lclass) {
                return;
            }
            break;
    }

    if(arg.type != expression_unresolved)
        errors->newerror(INCOMPATIBLE_TYPES, utype.lnk->line, utype.lnk->col, "; cannot cast `" + arg.utype.typeToString() + "` to `" + utype.utype.typeToString() + "`");
    return;
}

Expression RuntimeEngine::parseCastExpression(Ast* pAst) {
    Expression expression, utype, arg;

    utype = parseUtype(pAst->getsubast(ast_utype));
    arg = parseExpression(pAst->getsubast(ast_expression));

    if(utype.type != expression_unknown &&
       utype.type != expression_unresolved) {
        switch(utype.type) {
            case expression_native:
                expression.code.inject(expression.code.size(), arg.code);
                parseNativeCast(utype, arg, expression);
                break;
            case expression_class:
                parseClassCast(utype, arg, expression);
                expression.func=arg.func;
                break;
            case expression_field:
                errors->newerror(GENERIC, utype.lnk->line, utype.lnk->col, "cast expression of type `field` not allowed, must be of type `class`");
                break;
            default:
                errors->newerror(GENERIC, utype.lnk->line, utype.lnk->col, "cast expression of type `" + utype.typeToString() + "` not allowed, must be of type `class`");
                break;
        }
    }

    expression.newExpression=arg.newExpression;
    return expression;
}

void RuntimeEngine::preIncClass(Expression& out, token_entity op, ClassObject* klass) {
    OperatorOverload* overload;
    List<Param> emptyParams;

    if(op.gettokentype() == _INC) {
        overload = klass->getPreIncOverload();
    } else {
        overload = klass->getPreDecOverload();
    }

    if(overload != NULL) {
        // add code to call overload

        out.code.push_i64(SET_Di(i64, op_MOVI, 0), ebx);
        out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));

        if(!overload->isStatic())
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

        out.code.push_i64(SET_Di(i64, op_CALL, overload->address));

        out.type = methodReturntypeToExpressionType(overload);
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
        }
    } else if(klass->hasOverload(stringToOp(op.gettoken()))) {
        errors->newerror(GENERIC, op.getline(), op.getcolumn(), "use of class `" + klass->getName() + "`; missing overload params for operator `"
                                                                + klass->getFullName() + ".operator" + op.gettoken() + "`");
        out.type = expression_var;
    } else {
        errors->newerror(GENERIC, op.getline(), op.getcolumn(), "use of class `" + klass->getName() + "` must return an int to use `" + op.gettoken() + "` operator");
        out.type = expression_var;
    }
}

Expression RuntimeEngine::parsePreInc(Ast* pAst) {
    Expression interm;
    token_entity entity = pAst->hasentity(_INC) ? pAst->getentity(_INC) : pAst->getentity(_DEC);

    interm = parseExpression(pAst->getsubast(0));
    Expression expression(interm);

    if(expression.utype.array){
        errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "expression must evaluate to an int to use `" + entity.gettoken() + "` operator");
    } else {
        switch(expression.type) {
            case expression_var:
                if(interm.func) {
                    pushExpressionToRegister(interm, expression, ebx);
                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                    else
                        expression.code.push_i64(SET_Di(i64, op_DEC, ebx));
                } else {
                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                    else
                        expression.code.push_i64(SET_Di(i64, op_DEC, ebx));
                }
                break;
            case expression_field:
                if(interm.utype.field->type == CLASS) {
                    if(expression.utype.field->local)
                        expression.code.push_i64(SET_Di(i64, op_MOVL, expression.utype.field->address));

                    preIncClass(expression, entity, expression.utype.field->klass);
                    return expression;
                } else if(interm.utype.field->type == VAR) {
                    // increment the field
                    expression.code.push_i64(SET_Di(i64, op_MOVI, 1), ecx);

                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(
                                SET_Ci(i64, op_ADDL, ecx, 0, interm.utype.field->address));
                    else
                        expression.code.push_i64(
                                SET_Ci(i64, op_SUBL, ecx, 0, interm.utype.field->address));

                    if(entity.gettokentype() == _INC)
                        expression.code.push_i64(SET_Di(i64, op_INC, ebx));
                    else
                        expression.code.push_i64(SET_Di(i64, op_DEC, ebx));

                } else if(interm.utype.field->type == OBJECT) {
                    errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "use of `" + entity.gettoken() +
                                                                                    "` operator on field of type `dynamic_object` without a cast. Try ((SomeClass)dynamic_class)++");

                } else if(interm.utype.field->type != UNDEFINED){
                    errors->newerror(GENERIC, entity.getline(), entity.getcolumn(),
                                     "expression must evaluate to an int to use `" + entity.gettoken() +
                                     "` operator");
                }
                break;
            case expression_lclass:
                if(interm.func)
                    expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
                preIncClass(expression, entity, expression.utype.klass);
                return expression;
                break;
            case expression_objectclass:
                errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "use of `" + entity.gettoken() + "` operator on type `dynamic_object` without a cast. Try ((SomeClass)dynamic_class)++");
                break;
            case expression_null:
                errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "value `null` cannot be used as var");
                break;
            case expression_native:
                errors->newerror(UNEXPECTED_SYMBOL, entity.getline(), entity.getcolumn(), " `" + nativefield_tostr(expression.utype.nf) + "`");
                break;
            case expression_string:
                errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "increment on immutable string");
                break;
            case expression_unresolved:
                // do nothing
                break;
            default:
                errors->newerror(GENERIC, entity.getline(), entity.getcolumn(), "expression must evaluate to an int to use `" + entity.gettoken() + "` operator");
                break;
        }
    }

    expression.type = expression_var;
    return expression;
}

Expression RuntimeEngine::parseParenExpression(Ast* pAst) {
    Expression expression;

    expression = parseExpression(pAst);

    if(pAst->hassubast(ast_dotnotation_call_expr)) {
        return parseDotNotationChain(pAst->getsubast(ast_dotnotation_call_expr), expression, 0);
    }

    return expression;
}

void RuntimeEngine::notClass(Expression& out, ClassObject* klass, Ast* pAst) {
    List<Param> emptyParams;
    Method* overload;
    if((overload = klass->getOverload(oper_NOT, emptyParams)) != NULL) {
        // call overload operator

        if(!overload->isStatic())
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

        out.code.push_i64(SET_Di(i64, op_CALL, overload->address));

        out.type = methodReturntypeToExpressionType(overload);
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
        }
    } else if(klass->hasOverload(oper_NOT)) {
        errors->newerror(GENERIC, pAst->line, pAst->col, "use of unary operator '!' on class `" + klass->getName() + "`; missing overload params for operator `"
                                                         + klass->getFullName() + ".operator!`");
    } else {
        // error cannot apply not to expression of type class
        errors->newerror(GENERIC, pAst->line, pAst->col, "unary operator '!' cannot be applied to expression of type `" + klass->getFullName() + "`");
    }
}

Expression RuntimeEngine::parseNotExpression(Ast* pAst) {
    Expression expression;

    expression = parseExpression(pAst);
    switch(expression.type) {
        case expression_var:
            // negate value
            if(expression.func)
                pushExpressionToRegisterNoInject(expression, expression, ebx);

            expression.code.push_i64(SET_Ci(i64, op_NOT, ebx,0, ebx));
            break;
        case expression_lclass:
            // check for !operator
            if(expression.func)
                expression.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            notClass(expression, expression.utype.klass, pAst);
            break;
        case expression_unresolved:
            // do nothing
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + expression.utype.toString() + "`");
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + expression.utype.typeToString() + "`");
            break;
        case expression_dynamicclass:
            errors->newerror(GENERIC, pAst->line, pAst->col, "unary operator '!' cannot be applied to dynamic_object, did you forget to add a cast?  i.e !((SomeClass)dynamic_class)");
            break;
        case expression_field:
            expression.type = expression_var;
            if(expression.utype.field->isNative()) {
                if(expression.utype.field->dynamicObject()) {
                    errors->newerror(GENERIC, pAst->line, pAst->col, "use of `!` operator on field of type `dynamic_object` without a cast. Try !((SomeClass)dynamic_class)");
                } else if(expression.utype.field->isVar()) {
                    pushExpressionToRegisterNoInject(expression, expression, ebx);
                    expression.code.push_i64(SET_Ci(i64, op_NOT, ebx,0, ebx));
                }
            } else if(expression.utype.field->type == CLASS) {
                if(expression.utype.field->local)
                    expression.code.push_i64(SET_Di(i64, op_MOVL, expression.utype.field->address));
                notClass(expression, expression.utype.field->klass, pAst);
            } else {
                errors->newerror(GENERIC, pAst->line, pAst->col, "field must evaluate to an int to use `!` operator");
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line, pAst->col, "unary operator '!' cannot be applied to null");
            break;
        case expression_string:
            errors->newerror(GENERIC, pAst->line, pAst->col, "unary operator '!' cannot be applied to immutable string");
            break;
        default:
            errors->newerror(GENERIC, pAst->line, pAst->col, "unary operator '!' cannot be applied to given expression");
            break;

    }

    return expression;
}

Expression RuntimeEngine::parseUnary(token_entity operand, Expression& right, Ast* pAst) {
    Expression expression;

    switch(right.type) {
        case expression_var:
            if(c_options.optimize && right.literal) {
                double var = 0;

                if(operand == "+")
                    var = +right.intValue;
                else
                    var = -right.intValue;

                if((((int64_t)abs(right.intValue)) - abs(right.intValue)) > 0) {
                    // movbi
                    if((int64_t )var > DA_MAX || (int64_t )var < DA_MIN) {
                        stringstream ss;
                        ss << "integral number too large: " << var;
                        errors->newerror(GENERIC, operand.getline(), operand.getcolumn(), ss.str());
                    }

                    expression.code.push_i64(SET_Di(i64, op_MOVBI, ((int64_t)var)), abs(get_low_bytes(var)));
                    expression.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, bmr));
                } else {
                    // movi
                    if(var > DA_MAX || var < DA_MIN) {
                        stringstream ss;
                        ss << "integral number too large: " << var;
                        errors->newerror(GENERIC, operand.getline(), operand.getcolumn(), ss.str());
                    }
                    expression.code.push_i64(SET_Di(i64, op_MOVI, var), ebx);
                }
            } else {
                expression.code.inject(expression.code.size(), right.code);

                if(operand == "+")
                    expression.code.push_i64(SET_Ci(i64, op_IMUL, ebx,0, 1));
                else
                    expression.code.push_i64(SET_Ci(i64, op_IMUL, ebx,0, -1));
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Unary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + right.typeToString() + "`");
            break;
        case expression_field:
            if(right.utype.field->isNative()) {
                // add var
                if(right.utype.field->isVar()) {
                    expression.code.inject(expression.code.size(), right.code); // ToDO: shouldnt we be pushing this to ebx register???

                    if(operand == "+") {
                        expression.code.push_i64(SET_Ci(i64, op_IMUL, ebx,0, 1));
                    }
                    else {
                        expression.code.push_i64(SET_Ci(i64, op_IMUL, ebx,0, -1));
                    }
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Unary operator `" + operand.gettoken() +
                                                                      "` cannot be applied to expression of type `" + right.typeToString() + "`");
                }
            } else if(right.utype.field->type == CLASS) {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Expression of type `" + right.typeToString() + "` is non numeric");
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + right.typeToString() + "`");
            break;
        case expression_lclass:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Expression of type `" + right.typeToString() + "` is non numeric");
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + right.typeToString() + "`");
            break;
        case expression_void:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Unary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + right.typeToString() + "`");
            break;
        case expression_objectclass:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Unary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + right.typeToString() + "`");
            break;
        case expression_string:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Expression of type `" + right.typeToString() + "` is non numeric");
            break;
        default:
            break;
    }

    expression.type = expression_var;
    expression.link = pAst;
    return expression;
}

bool RuntimeEngine::addExpressions(Expression &out, Expression &leftExpr, Expression &rightExpr, token_entity operand, double* varout) {
    if(operand == "+")
        *varout = leftExpr.intValue + rightExpr.intValue;
    else if(operand == "-")
        *varout = leftExpr.intValue - rightExpr.intValue;
    else if(operand == "*")
        *varout = leftExpr.intValue * rightExpr.intValue;
    else if(operand == "/")
        *varout = leftExpr.intValue / rightExpr.intValue;
    else if(operand == "%")
        *varout = (int64_t)leftExpr.intValue % (int64_t)rightExpr.intValue;

    if(!isDClassNumberEncodable(*varout)) {
        return false;
    } else {
        if(out.code.size() >= 2 && (GET_OP(out.code.__asm64.get(out.code.size()-2)) == op_MOVI
                                    || GET_OP(out.code.__asm64.get(out.code.size()-2)) == op_MOVBI)){
            out.code.__asm64.pop_back();
            out.code.__asm64.pop_back();
        }

        if((((int64_t)abs(*varout)) - abs(*varout)) > 0) {
            // movbi
            out.code.push_i64(SET_Di(i64, op_MOVBI, ((int64_t)*varout)), abs(get_low_bytes(*varout)));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, bmr));
        } else {
            // movi
            out.code.push_i64(SET_Di(i64, op_MOVI, *varout), ebx);
        }

        leftExpr.literal = true;
        leftExpr.intValue = *varout;
        leftExpr.code.free();
    }

    out.literal = true;
    out.intValue = *varout;
    return true;
}

Opcode RuntimeEngine::operandToOp(token_entity operand)
{
    if(operand == "+")
        return op_ADD;
    else if(operand == "-")
        return op_SUB;
    else if(operand == "*")
        return op_MUL;
    else if(operand == "/")
        return op_DIV;
    else if(operand == "%")
        return op_MOD;
    else
        return op_MOD;
}

bool RuntimeEngine::equals(Expression& left, Expression& right, string msg) {

    if(left.type == expression_native) {
        errors->newerror(UNEXPECTED_SYMBOL, left.lnk->line, left.lnk->col, " `" + left.typeToString() + "`" + msg);
        return false;
    }
    if(right.type == expression_native) {
        errors->newerror(UNEXPECTED_SYMBOL, right.lnk->line, right.lnk->col, " `" + right.typeToString() + "`" + msg);
        return false;
    }

    switch(left.type) {
        case expression_var:
            if(right.type == expression_var) {
                // add 2 vars
                return true;
            }
            else if(right.type == expression_field) {
                if(right.utype.field->isVar()) {
                    return true;
                }
            }
            break;
        case expression_null:
            if(right.type == expression_lclass || right.type == expression_objectclass) {
                return true;
            } else if(right.type == expression_class) {
                errors->newerror(GENERIC, right.lnk->line,  right.lnk->col, "Class `" + right.typeToString() + "` must be lvalue" + msg);
                return false;
            }
            break;
        case expression_field:
            if(left.utype.field->isNative()) {
                // add var
                if(right.type == expression_var) {
                    if(left.utype.field->isVar()) {
                        return true;
                    }
                } else if(right.type == expression_objectclass) {
                    if(left.utype.field->dynamicObject()) {
                        return true;
                    }
                } else if(right.type == expression_field) {
                    if(right.utype.field->isVar()) {
                        return left.utype.field->isVar();
                    }
                    else if(right.utype.field->dynamicObject()) {
                        return left.utype.field->dynamicObject();
                    }
                } else if(right.type == expression_string) {
                    return left.utype.field->isArray;
                } else if(right.type == expression_lclass) {
                    return left.utype.field->dynamicObject();
                }
            } else if(left.utype.field->type == CLASS) {
                if(right.type == expression_lclass) {
                    if(left.utype.field->klass->match(right.utype.klass)) {
                        return true;
                    }
                } else if(right.type == expression_class) {
                    if(left.utype.field->klass->match(right.utype.klass)) {
                        errors->newerror(GENERIC, right.lnk->line,  right.lnk->col, "Class `" + right.typeToString() + "` must be lvalue" + msg);
                        return false;
                    }
                } else if(right.type == expression_field && right.utype.field->type == CLASS) {
                    if(right.utype.field->klass->match(left.utype.field->klass)) {
                        return true;
                    }
                } else {
                    List<Param> params;
                    List<Expression> exprs;
                    exprs.push_back(right);

                    expressionListToParams(params, exprs);
                    return left.utype.field->klass->getOverload(oper_EQUALS, params) != NULL;
                }
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_lclass:
            if(right.type == expression_lclass) {
                if(left.utype.klass->match(right.utype.klass)) {
                    return true;
                }
            } else if(right.type == expression_field) {
                if(left.utype.klass->match(right.utype.field->klass)) {
                    return true;
                }
            } else if(right.type == expression_class) {
                if(left.utype.klass->match(right.utype.klass)) {
                    errors->newerror(GENERIC, right.lnk->line,  right.lnk->col, "Class `" + right.typeToString() + "` must be lvalue" + msg);
                    return false;
                }
            }
            break;
        case expression_class:
            if(right.type == expression_class) {
                if(left.utype.klass->match(right.utype.klass)) {
                    return true;
                }
            }
            break;
        case expression_void:
            if(right.type == expression_void) {
                return true;
            }
            break;
        case expression_objectclass:
            if(right.type == expression_objectclass || right.type == expression_lclass) {
                return true;
            } else if(right.type == expression_field) {
                if(right.utype.field->type == OBJECT || right.utype.field->type == CLASS) {
                    return true;
                }
            }
            break;
        case expression_string:
            if(right.type == expression_field) {
                if(right.utype.field->isVar() && right.utype.field->isArray) {
                    return true;
                }
            }
            else if(right.type == expression_var) {
                if(right.utype.array) {
                    return true;
                }
            }
            else if(right.type == expression_string) {
                return true;
            }
            break;
        default:
            break;
    }

    errors->newerror(GENERIC, right.lnk->line,  right.lnk->col, "Expressions of type `" + left.typeToString() + "` and `" + right.typeToString() + "` are not compatible" + msg);
    return false;
}

void RuntimeEngine::addNative(token_entity operand, FieldType type, Expression& out, Expression& left, Expression& right, Ast* pAst) {
    out.type = expression_var;
    right.type = expression_var;
    right.func=false;
    right.literal = false;
    Expression expr;

    if(left.type == expression_var) {
        equals(left, right);

        pushExpressionToRegister(right, out, egx);
        pushExpressionToRegister(left, out, ebx);
        out.code.push_i64(SET_Ci(i64, operandToOp(operand), ebx,0, egx), ebx);
        right.code.free();
    } else if(left.type == expression_field) {
        if(left.utype.field->isVar()) {
            equals(left, right);

            pushExpressionToRegister(right, out, egx); // no inject?
            pushExpressionToRegister(left, out, ebx);
            out.code.push_i64(SET_Ci(i64, operandToOp(operand), ebx,0, egx), ebx);
            right.code.free();
        }
        else {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
    }
}

void RuntimeEngine::addClass(token_entity operand, ClassObject* klass, Expression& out, Expression& left, Expression &right, Ast* pAst) {
    List<Param> params;
    List<Expression> eList;
    eList.push_back(right);
    OperatorOverload* overload;
    right.literal = false;

    expressionListToParams(params, eList);

    if((overload = klass->getOverload(stringToOp(operand.gettoken()), params)) != NULL) {
        // call operand
        if(!overload->isStatic())
            pushExpressionToStack(left, out);

        pushExpressionToStack(right, out);

        out.type = methodReturntypeToExpressionType(overload);
        right.free();
        right.type=out.type;
        right.func=true;
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
            right.utype.klass = overload->klass;
            right.utype.type = CLASS;
        }

        out.func=true;
        out.code.push_i64(SET_Di(i64, op_CALL, overload->address));
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `"
                                                          + left.typeToString() + "` and `" + right.typeToString() + "`");
    }

    freeList(params);
    freeList(eList);
}

void RuntimeEngine::addClassChain(token_entity operand, ClassObject* klass, Expression& out, Expression& left, Expression &right, Ast* pAst) {
    List<Param> params;
    List<Expression> eList;
    eList.push_back(right);
    OperatorOverload* overload;
    left.literal = false;

    expressionListToParams(params, eList);

    if((overload = klass->getOverload(stringToOp(operand.gettoken()), params)) != NULL) {
        // call operand
        if(overload->isStatic()) {
            errors->newerror(GENERIC, pAst, "call to function `$operator" + operand.gettoken() + "` is static");
        }


        if(left.func && left.type != expression_void) {
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Di(i64, op_DEC, sp));

            out.code.push_i64(SET_Di(i64, op_PUSHOBJ, 0));
        } else if(left.func) {
            errors->newerror(GENERIC, pAst, "call to function `$operator" + operand.gettoken() + "` returns void from previous operand");
        } else {
            out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));
        }

        pushExpressionToStack(right, out);

        out.type = methodReturntypeToExpressionType(overload);
        left.free();
        left.type=out.type;
        left.func=true;
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
            left.utype.klass = overload->klass;
            left.utype.type = CLASS;
        }

        out.func=true;
        out.code.push_i64(SET_Di(i64, op_CALL, overload->address));
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `"
                                                          + left.typeToString() + "` and `" + right.typeToString() + "`");
    }

    freeList(params);
    freeList(eList);
}

void RuntimeEngine::addStringConstruct(token_entity operand, ClassObject* klass, Expression& out, Expression& left, Expression &right, Ast* pAst) {
    List<Param> params;
    List<Expression> eList;
    eList.push_back(right);
    OperatorOverload* overload;
    right.literal = false;
    Expression newOut;

    expressionListToParams(params, eList);

    if((overload = klass->getOverload(stringToOp(operand.gettoken()), params)) != NULL) {
        // call operand

        newOut.inject(out);

        pushExpressionToStack(right, newOut);

        out.type = methodReturntypeToExpressionType(overload);
        left.free();
        left.type=out.type;
        left.func=true;
        if(out.type == expression_lclass) {
            out.utype.klass = overload->klass;
            out.utype.type = CLASS;
            left.utype.klass = overload->klass;
            left.utype.type = CLASS;
        }

        out.func=true;
        newOut.code.push_i64(SET_Di(i64, op_CALL, overload->address));

        out.code.free();
        out.inject(newOut);
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `"
                                                          + left.typeToString() + "` and `" + right.typeToString() + "`");
    }

    freeList(params);
    freeList(eList);
}

void RuntimeEngine::constructNewString(Expression &stringExpr, Expression& out) {
    List<Expression> expressions;
    List<Param> params;
    ClassObject* klass;
    Method* fn=NULL;

    expressions.add(stringExpr);
    expressionListToParams(params, expressions);

    if((klass = getClass("std", "String")) != NULL) {
        if((fn=klass->getConstructor(params)) != NULL) {
            out.type = expression_lclass;
            out.utype.klass = klass;
            out.utype.type=CLASS;

            out.code.push_i64(SET_Di(i64, op_NEWCLASS, klass->address));
            if(!fn->isStatic())
                out.code.push_i64(SET_Ei(i64, op_PUSHOBJ));

            for(unsigned int i = 0; i < expressions.size(); i++) {
                pushExpressionToStack(expressions.get(i), stringExpr);
            }
            out.code.push_i64(SET_Di(i64, op_CALL, fn->address));
        } else
            out = stringExpr;
    } else
        out = stringExpr;

    freeList(params);
    freeList(expressions);
}

bool RuntimeEngine::constructNewString(Expression &stringExpr, Expression &right, token_entity operand, Expression& out, Ast* pAst) {
    List<Expression> expressions;
    List<Param> params;
    ClassObject* klass;
    Method* fn=NULL;

    expressions.add(stringExpr);
    expressionListToParams(params, expressions);

    if((klass = getClass("std", "String")) != NULL) {
        if((fn=klass->getConstructor(params)) != NULL) {
            stringExpr.type = expression_lclass;
            stringExpr.utype.klass = klass;
            stringExpr.utype.type=CLASS;
            out.type = expression_lclass;
            out.utype.klass = klass;
            out.utype.type=CLASS;

            out.code.push_i64(SET_Di(i64, op_INC, sp));
            out.code.push_i64(SET_Di(i64, op_MOVSL, 0));
            out.code.push_i64(SET_Di(i64, op_NEWCLASS, klass->address));

            for(unsigned int i = 0; i < expressions.size(); i++) {
                pushExpressionToStack(expressions.get(i), out);
            }
            out.code.push_i64(SET_Di(i64, op_CALL, fn->address));

            addStringConstruct(operand, klass, out, stringExpr, right, pAst);

            out.func=true;
            freeList(params);
            freeList(expressions);
            return true;
        } else
            out = stringExpr;
    } else
        out = stringExpr;

    freeList(params);
    freeList(expressions);
    return false;
}

bool RuntimeEngine::isMathOp(token_entity entity)
{
    return entity == "+" || entity == "-"
           || entity == "*" || entity == "/"
           || entity == "%";
}

void RuntimeEngine::parseAddExpressionChain(Expression &out, Ast *pAst) {
    Expression leftExpr, rightExpr, peekExpr;
    int operandPtr = 0;
    double value=0;
    bool firstEval=true;
    token_entity operand;
    List<token_entity> operands;
    operands.add(pAst->getentity(0));
    for(unsigned int i = 0; i < pAst->getsubastcount(); i++) {
        if(pAst->getsubast(i)->getentitycount() > 0 && isMathOp(pAst->getsubast(i)->getentity(0)))
            operands.add(pAst->getsubast(i)->getentity(0));
    }

    operandPtr=0;
    for(long int i = 0; i < pAst->getsubastcount(); i++) {
        if(firstEval) {
            leftExpr = pAst->getsubast(i)->gettype() == ast_expression ? parseExpression(pAst->getsubast(i))
                                                                       : parseIntermExpression(pAst->getsubast(i));
            i++;
            firstEval= false;
        }
        rightExpr = pAst->getsubast(i)->gettype() == ast_expression ? parseExpression(pAst->getsubast(i)) :
                    parseIntermExpression(pAst->getsubast(i));
        operand = operands.get(operandPtr++);
        double var = 0;


        switch(leftExpr.type) {
            case expression_var:
                if(rightExpr.type == expression_var) {
                    if(leftExpr.literal && rightExpr.literal) {

                        if(!addExpressions(out, leftExpr, rightExpr, operand, &var)) {
                            goto calculate;
                        }
                    } else {
                        calculate:
                        // is left leftexpr a literal?
                        pushExpressionToStack(rightExpr, out);
                        pushExpressionToRegister(leftExpr, out, ebx);
                        out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                        out.code.push_i64(SET_Ci(i64, operandToOp(operand), ebx,0, ecx), ebx);

                        leftExpr.func=false;
                        leftExpr.literal = false;
                        leftExpr.code.free();
                        var=0;
                    }

                    out.type = expression_var;
                }else if(rightExpr.type == expression_field) {
                    if(rightExpr.utype.field->isNative()) {
                        // add var
                        leftEval:
                        if(leftExpr.literal && (i-1) >= 0) {
                            peekExpr = pAst->getsubast(i-1)->gettype() == ast_expression ?
                                       parseExpression(pAst->getsubast(i-1)) : parseIntermExpression(pAst->getsubast(i-1));
                            Expression outtmp;

                            if(peekExpr.literal) {
                                if(addExpressions(outtmp, peekExpr, leftExpr, operand, &var)) {
                                    i--;
                                    leftExpr=outtmp;
                                    leftExpr.type=expression_var;
                                    goto leftEval;
                                }
                            }
                        }

                        addNative(operand, rightExpr.utype.field->type, out, leftExpr, rightExpr, pAst);
                    } else if(rightExpr.utype.field->type == CLASS) {
                        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                          "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                    } else {
                        // do nothing field unresolved
                    }
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                      "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                }
                break;
            case expression_null:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                break;
            case expression_field:
                if(leftExpr.utype.field->isNative()) {
                    // add var
                    addNative(operand, leftExpr.utype.field->type, out, leftExpr, rightExpr, pAst);
                } else if(leftExpr.utype.field->type == CLASS) {
                    if(i <= 1) {
                        addClass(operand, leftExpr.utype.field->klass, out, leftExpr, rightExpr, pAst);
                        leftExpr=rightExpr;
                    }
                    else {
                        addClassChain(operand, leftExpr.utype.field->klass, out, leftExpr, rightExpr, pAst);
                    }
                } else {
                    // do nothing field unresolved
                }
                break;
            case expression_native:
                errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + leftExpr.typeToString() + "`");
                break;
            case expression_lclass:
                if(i <= 1) {
                    addClass(operand, leftExpr.utype.klass, out, leftExpr, rightExpr, pAst);
                    leftExpr=rightExpr;
                }
                else {
                    addClassChain(operand, leftExpr.utype.klass, out, leftExpr, rightExpr, pAst);
                }
                break;
            case expression_class:
                errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + leftExpr.typeToString() + "`");
                break;
            case expression_void:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                break;
            case expression_objectclass:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`. Did you forget to apply a cast? "
                                                                          "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
                break;
            case expression_string:

                constructNewString(leftExpr, rightExpr, operand, out, leftExpr.lnk);
                break;
            default:
                break;
        }
    }

    /*
     * So we dont missinterpret the value returned from the expr :)
     */
    out.func=leftExpr.func;
    out.literal=leftExpr.literal=false;
}

Expression RuntimeEngine::parseAddExpression(Ast* pAst) {
    Expression expression, right;
    token_entity operand = pAst->hasentity(PLUS) ? pAst->getentity(PLUS) : pAst->getentity(MINUS);

    if(operand.gettokentype() == PLUS && pAst->getsubastcount() == 1) {
        // left is right then add 1 to data
        right = parseExpression(pAst);
        return parseUnary(operand, right, pAst);
    } else if(operand.gettokentype() == MINUS && pAst->getsubastcount() == 1) {
        // left is right multiply expression by -1
        right = parseExpression(pAst);
        return parseUnary(operand, right, pAst);
    }

    parseAddExpressionChain(expression, pAst);

    expression.lnk = pAst;
    return expression;
}

Expression RuntimeEngine::parseMultExpression(Ast* pAst) {
    Expression expression, left, right;
    token_entity operand = pAst->getentity(0);

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary *<expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    parseAddExpressionChain(expression, pAst);

    expression.link = pAst;
    return expression;
}

bool RuntimeEngine::shiftLiteralExpressions(Expression &out, Expression &leftExpr, Expression &rightExpr,
                                      token_entity operand) {
    double var=0;
    if(operand == "<<")
        var = (int64_t)leftExpr.intValue << (int64_t)rightExpr.intValue;
    else if(operand == ">>")
        var = (int64_t)leftExpr.intValue >> (int64_t)rightExpr.intValue;

    if(isDClassNumberEncodable(var)) {
        return false;
    } else {
        if((((int64_t)abs(var)) - abs(var)) > 0) {
            // movbi
            out.code.push_i64(SET_Di(i64, op_MOVBI, ((int64_t)var)), abs(get_low_bytes(var)));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, bmr));
        } else {
            // movi
            out.code.push_i64(SET_Di(i64, op_MOVI, var), ebx);
        }

        rightExpr.literal = true;
        rightExpr.intValue = var;
        rightExpr.code.free();
    }

    out.literal = true;
    out.intValue = var;
    return true;
}

Opcode RuntimeEngine::operandToShftOp(token_entity operand)
{
    if(operand == "<<")
        return op_SHL;
    else
        return op_SHR;
}

void RuntimeEngine::shiftNative(token_entity operand, Expression& out, Expression &left, Expression &right, Ast* pAst) {
    out.type = expression_var;
    right.type = expression_var;
    right.func=false;
    right.literal = false;

    if(left.type == expression_var) {
        equals(left, right);

        pushExpressionToRegister(right, out, egx);
        pushExpressionToRegister(left, out, ebx);
        out.code.push_i64(SET_Ci(i64, operandToShftOp(operand), ebx,0, egx), ebx);
        right.code.free();
    } else if(left.type == expression_field) {
        if(left.utype.field->isVar()) {
            equals(left, right);

            pushExpressionToRegister(right, out, egx); // no inject?
            pushExpressionToRegister(left, out, ebx);
            out.code.push_i64(SET_Ci(i64, operandToShftOp(operand), ebx,0, egx), ebx);
            right.code.free();
        }
        else {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
    }
}

Expression RuntimeEngine::parseShiftExpression(Ast* pAst) {
    Expression out, left, right;
    token_entity operand = pAst->getentity(0);

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary << <expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    left = parseIntermExpression(pAst->getsubast(0));
    right = parseExpression(pAst->getsubast(1));

    out.type = expression_var;
    switch(left.type) {
        case expression_var:
            if(right.type == expression_var) {
                if(left.literal && right.literal) {

                    if(!shiftLiteralExpressions(out, left, right, operand)) {
                        goto calculate;
                    }
                } else {
                    calculate:
                    // is left left expr a literal?
                    pushExpressionToStack(right, out);
                    pushExpressionToRegister(left, out, ebx);
                    out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                    out.code.push_i64(SET_Ci(i64, operandToShftOp(operand), ebx,0, ecx), ebx);
                }
            }
            else if(right.type == expression_field) {
                if(right.utype.field->isNative()) {
                    // add var
                    shiftNative(operand, out, left, right, pAst);
                    //addNative(operand, right.utype.field->nf, expression, left, right, pAst);
                } else if(right.utype.field->type == CLASS) {
                    addClass(operand, right.utype.field->klass, out, left, right, pAst);
                } else {
                    // do nothing field unresolved
                }
            } else if(right.type == expression_lclass) {
                addClass(operand, left.utype.klass, out, left, right, pAst);
            } else {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        case expression_field:
            if(left.utype.field->isVar()) {
                // add var
                shiftNative(operand, out, left, right, pAst);
            } else if(left.utype.field->type == CLASS) {
                addClass(operand, left.utype.field->klass, out, left, right, pAst);
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_lclass:
            addClass(operand, left.utype.klass, out, left, right, pAst);
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_void:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        case expression_objectclass:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`. Did you forget to apply a cast? "
                                                                      "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
            break;
        case expression_string:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        default:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
    }

    out.lnk = pAst;
    return out;
}

void RuntimeEngine::lessThanLiteralExpressions(Expression &out, Expression &leftExpr, Expression &rightExpr,
                                         token_entity operand) {
    double var=0;
    if(operand == "<")
        var = leftExpr.intValue < rightExpr.intValue;
    else if(operand == ">")
        var = leftExpr.intValue > rightExpr.intValue;
    else if(operand == ">=")
        var = leftExpr.intValue >= rightExpr.intValue;
    else if(operand == "<=")
        var = leftExpr.intValue <= rightExpr.intValue;

    out.code.push_i64(SET_Di(i64, op_MOVI, var), ebx);

    rightExpr.literal = true;
    rightExpr.intValue = var;
    rightExpr.code.free();

    out.literal = true;
    out.intValue = var;
}

Opcode RuntimeEngine::operandToLessOp(token_entity operand)
{
    if(operand == "<")
        return op_LT;
    else if(operand == "<=")
        return op_LTE;
    else if(operand == ">")
        return op_GT;
    else if(operand == ">=")
        return op_GTE;
    else
        return op_LT;
}

void RuntimeEngine::lessThanNative(token_entity operand, Expression& out, Expression &left, Expression &right, Ast* pAst) {
    out.type = expression_var;
    right.type = expression_var;
    right.func=false;
    right.literal = false;

    if(left.type == expression_var) {
        equals(left, right);

        pushExpressionToRegister(right, out, egx);
        pushExpressionToRegister(left, out, ebx);
        out.code.push_i64(SET_Ci(i64, operandToLessOp(operand), ebx,0, egx));
        out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
        right.code.free();
    } else if(left.type == expression_field) {
        if(left.utype.field->isVar()) {
            equals(left, right);

            pushExpressionToRegister(right, out, egx);
            pushExpressionToRegister(left, out, ebx);
            out.code.push_i64(SET_Ci(i64, operandToLessOp(operand), ebx,0, egx));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
            right.code.free();
        }
        else {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
    }
}

Expression RuntimeEngine::parseLessExpression(Ast* pAst) {
    Expression out, left, right;
    token_entity operand = pAst->getentity(0);

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary < <expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    left = parseIntermExpression(pAst->getsubast(0));
    right = parseExpression(pAst->getsubast(1));

    out.type = expression_var;
    switch(left.type) {
        case expression_var:
            if(right.type == expression_var) {
                if(left.literal && right.literal) {
                    lessThanLiteralExpressions(out, left, right, operand);
                } else {
                    calculate:
                    // is left left expr a literal?
                    pushExpressionToStack(right, out);
                    pushExpressionToRegister(left, out, ebx);
                    out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                    out.code.push_i64(SET_Ci(i64, operandToLessOp(operand), ebx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                }
            }
            else if(right.type == expression_field) {
                if(right.utype.field->isVar()) {
                    // add var
                    lessThanNative(operand, out, left, right, pAst);
                } else if(right.utype.field->type == CLASS) {
                    addClass(operand, right.utype.field->klass, out, left, right, pAst);
                } else {
                    // do nothing field unresolved
                }
            } else if(right.type == expression_lclass) {
                addClass(operand, left.utype.klass, out, left, right, pAst);
            } else {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        case expression_field:
            if(left.utype.field->isVar()) {
                // add var
                lessThanNative(operand, out, left, right, pAst);
            } else if(left.utype.field->type == CLASS) {
                addClass(operand, left.utype.field->klass, out, left, right, pAst);
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_lclass:
            addClass(operand, left.utype.klass, out, left, right, pAst);
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_void:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        case expression_objectclass:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`. Did you forget to apply a cast? "
                                                                      "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
            break;
        case expression_string:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        default:
            break;
    }

    out.lnk = pAst;
    return out;
}

bool RuntimeEngine::equalsNoErr(Expression& left, Expression& right) {

    if(left.type == expression_native) {
        return false;
    }
    if(right.type == expression_native) {
        return false;
    }

    switch(left.type) {
        case expression_var:
            if(right.type == expression_var) {
                // add 2 vars
                return true;
            }
            else if(right.type == expression_field) {
                if(right.utype.field->isNative()) {
                    if(right.utype.field->isVar()) {
                        return !right.utype.field->isArray;
                    }
                }
            }
            break;
        case expression_null:
            if(right.type == expression_lclass || right.type == expression_objectclass) {
                return true;
            } else if(right.type == expression_class) {
                return false;
            }
            break;
        case expression_field:
            if(left.utype.field->isNative()) {
                // add var
                if(right.type == expression_var) {
                    if(left.utype.field->isVar()) {
                        return !left.utype.array;
                    }
                } else if(right.type == expression_objectclass) {
                    if(left.utype.field->dynamicObject()) {
                        return true;
                    }
                } else if(right.type == expression_field) {
                    if(right.utype.field->isVar()) {
                        return left.utype.field->isVar() && left.utype.field->isArray==right.utype.field->isArray;
                    }
                    else if(right.utype.field->dynamicObject()) {
                        return left.utype.field->dynamicObject();
                    }
                } else if(right.type == expression_string) {
                    return left.utype.field->isVar() && left.utype.field->isArray;
                } else if(right.type == expression_lclass) {
                    return left.utype.field->dynamicObject();
                }
            } else if(left.utype.field->type == CLASS) {
                if(right.type == expression_lclass) {
                    if(left.utype.field->klass->match(right.utype.klass)) {
                        return true;
                    }
                } else if(right.type == expression_class) {
                    if(left.utype.field->klass->match(right.utype.klass)) {
                        return false;
                    }
                } else if(right.type == expression_field && right.utype.field->type == CLASS) {
                    if(right.utype.field->klass->match(left.utype.field->klass)) {
                        return true;
                    }
                }
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_lclass:
            if(right.type == expression_lclass) {
                if(left.utype.klass->match(right.utype.klass)) {
                    return true;
                }
            } else if(right.type == expression_field) {
                if(left.utype.klass->match(right.utype.field->klass)) {
                    return true;
                }
            } else if(right.type == expression_class) {
                if(left.utype.klass->match(right.utype.klass)) {
                    return false;
                }
            }
            break;
        case expression_class:
            if(right.type == expression_class) {
                if(left.utype.klass->match(right.utype.klass)) {
                    return true;
                }
            }
            break;
        case expression_void:
            if(right.type == expression_void) {
                return true;
            }
            break;
        case expression_objectclass:
            if(right.type == expression_objectclass || right.type == expression_lclass) {
                return true;
            } else if(right.type == expression_field) {
                if(right.utype.field->type == OBJECT || right.utype.field->type == CLASS) {
                    return true;
                }
            }
            break;
        case expression_string:
            if(right.type == expression_field) {
                if(right.utype.field->isVar() && right.utype.field->isArray) {
                    return true;
                }
            }
            else if(right.type == expression_var) {
                if(right.utype.array) {
                    return true;
                }
            }
            else if(right.type == expression_string) {
                return true;
            }
            break;
        default:
            break;
    }

    return false;
}

void RuntimeEngine::assignValue(token_entity operand, Expression& out, Expression &left, Expression &right, Ast* pAst) {
    out.type=expression_var;
    out.literal=false;

    /*
     * We know when this is called the operand is going to be =
     */
    if((left.type == expression_var && !left.arrayElement) || left.literal) {
        errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable, expression must be of lvalue");
    } else if(left.type == expression_field) {
        if(left.utype.field->isObjectInMemory()) {
            memassign:
            if(left.utype.field->isArray && operand != "=" && right.type != expression_null) {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                                  + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            }

            if(operand == "=" && right.type == expression_null) {
                out.code.inject(out.code.__asm64.size(), left.code);
                out.code.push_i64(SET_Ei(i64, op_DEL));
                return;
            } else if(left.utype.field->type != CLASS && right.type == expression_string) {
                pushExpressionToStack(right, out);
                out.code.inject(out.code.__asm64.size(), left.code);

                out.code.push_i64(SET_Ei(i64, op_POPOBJ));
                return;
            } else if(operand == "==" && right.type == expression_null) {
                pushExpressionToPtr(left, out);

                out.code.push_i64(SET_Ei(i64, op_CHECKNULL));
                out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                return;
            } else if(operand == "!=" && right.type == expression_null) {
                pushExpressionToPtr(left, out);

                out.code.push_i64(SET_Ei(i64, op_CHECKNULL));
                out.code.push_i64(SET_Ci(i64, op_NOT, cmt,0, cmt));
                out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                return;
            } else if(right.type == expression_null) {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                                  + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            }

            if(equalsNoErr(left, right)) {
                if(left.utype.field->isVar()) {}
                else if(left.utype.field->dynamicObject()) {
                    out.type=expression_objectclass;
                }else if(left.utype.field->type == CLASS) {
                    out.type=expression_lclass;
                    out.utype.klass=left.utype.field->klass;
                }

                if(c_options.optimize && right.utype.type == CLASSFIELD
                   && right.utype.field->local) {
                    out.inject(left);
                    out.code.push_i64(SET_Di(i64, op_MULL, right.utype.field->address));
                } else {
                    pushExpressionToStack(right, out);
                    out.inject(left);
                    out.code.push_i64(SET_Ei(i64, op_POPOBJ));
                }
            } else {
                if(left.type == expression_lclass) {
                    addClass(operand, left.utype.klass, out, left, right, pAst);
                } else if(left.utype.type == CLASSFIELD && left.utype.field->type == CLASS) {
                    addClass(operand, left.utype.field->klass, out, left, right, pAst);
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                                      + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
                }
            }
        } else {
            // this will b easy...
            if(left.utype.field->local) {
                pushExpressionToRegister(right, out, ecx);

                if(operand == "=") {
                    out.code.push_i64(SET_Ci(i64, op_MOVR, adx,0, fp));
                    out.code.push_i64(SET_Ci(i64, op_SMOVR, ecx,0, left.utype.field->address));
                } else if(operand == "+=") {
                    out.code.push_i64(SET_Ci(i64, op_ADDL, ecx,0, left.utype.field->address));
                } else if(operand == "-=") {
                    out.code.push_i64(SET_Ci(i64, op_SUBL, ecx,0, left.utype.field->address));
                } else if(operand == "*=") {
                    out.code.push_i64(SET_Ci(i64, op_MULL, ecx,0, left.utype.field->address));
                } else if(operand == "/=") {
                    out.code.push_i64(SET_Ci(i64, op_DIVL, ecx,0, left.utype.field->address));
                } else if(operand == "%=") {
                    out.code.push_i64(SET_Ci(i64, op_MODL, ecx,0, left.utype.field->address));
                } else if(operand == "&=") {
                    out.code.push_i64(SET_Ci(i64, op_ANDL, ecx,0, left.utype.field->address));
                } else if(operand == "|=") {
                    out.code.push_i64(SET_Ci(i64, op_ORL, ecx,0, left.utype.field->address));
                } else if(operand == "^=") {
                    out.code.push_i64(SET_Ci(i64, op_NOTL, ecx,0, left.utype.field->address));
                }

            } else {
                pushExpressionToStack(right, out);
                if(operand == "=")
                    out.inject(left);
                else
                    pushExpressionToRegister(left, out, egx);


                out.code.push_i64(SET_Di(i64, op_MOVI, 0), adx);
                out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));

                if(operand == "=") {
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                } else if(operand == "+=") {
                    out.code.push_i64(SET_Ci(i64, op_ADD, egx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                } else if(operand == "-=") {
                    out.code.push_i64(SET_Ci(i64, op_SUB, egx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                } else if(operand == "*=") {
                    out.code.push_i64(SET_Ci(i64, op_MUL, egx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                } else if(operand == "/=") {
                    out.code.push_i64(SET_Ci(i64, op_DIV, egx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                } else if(operand == "%=") {
                    out.code.push_i64(SET_Ci(i64, op_MOD, egx,0, ecx));
                    out.code.push_i64(SET_Ci(i64, op_RMOV, adx,0, ecx));
                }
            }
        }
    } else if(left.type == expression_lclass) {
        goto memassign;
    } else if(left.type == expression_var) {
        // this must be an array element
        if(left.arrayElement) {
            Expression e;
            pushExpressionToRegister(right, out, ebx);
            out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
            for(unsigned int i = 0; i < left.code.size(); i++)
            {
                if(GET_OP(left.code.__asm64.get(i)) == op_IALOAD_2) {
                    left.code.__asm64.remove(i);
                    unsigned  int pos = i;

                    left.code.__asm64.insert(i+1, SET_Ci(i64, op_SMOV, ebx,0, -1));
                    //left.code.__asm64.insert(pos+2, SET_Ci(i64, op_RMOV, ebx,0, egx)); we dont need this right?
                    break;
                }
            }
            out.inject(left);
            out.code.push_i64(SET_Ei(i64, op_POP));
        }
    } else if(left.type == expression_objectclass) {
        if(operand == "=" && right.type == expression_null) {
            out.code.inject(out.code.__asm64.size(), left.code);
            out.code.push_i64(SET_Ei(i64, op_DEL));
            return;
        } else if(right.type == expression_string) {
            pushExpressionToStack(right, out);
            out.code.inject(out.code.__asm64.size(), left.code);

            out.code.push_i64(SET_Ei(i64, op_POPOBJ));
            return;
        } else if(operand == "==" && right.type == expression_null) {
            pushExpressionToPtr(left, out);

            out.code.push_i64(SET_Ei(i64, op_CHECKNULL));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
            return;
        } else if(operand == "!=" && right.type == expression_null) {
            pushExpressionToPtr(left, out);

            out.code.push_i64(SET_Ei(i64, op_CHECKNULL));
            out.code.push_i64(SET_Ci(i64, op_NOT, cmt,0, cmt));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
            return;
        } else if(right.type == expression_null) {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                              + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }

        if(equalsNoErr(left, right)) {
            out.type=expression_objectclass;
            if(c_options.optimize && right.utype.type == CLASSFIELD
               && right.utype.field->local) {
                out.inject(left);
                out.code.push_i64(SET_Di(i64, op_MULL, right.utype.field->address));
            } else {
                pushExpressionToStack(right, out);
                out.inject(left);
                out.code.push_i64(SET_Ei(i64, op_POPOBJ));
            }
        } else {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                              + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken()
                                                          + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
    }

}

Opcode RuntimeEngine::operandToCompareOp(token_entity operand)
{
    if(operand == "!=")
        return op_TNE;
    else if(operand == "==")
        return op_TEST;
    else
        return op_TEST;
}

void RuntimeEngine::assignNative(token_entity operand, Expression& out, Expression &left, Expression &right, Ast* pAst) {
    out.type = expression_var;

    if(left.type == expression_var) {
        equals(left, right);

        pushExpressionToRegister(right, out, egx);
        pushExpressionToRegister(left, out, ebx);
        out.code.push_i64(SET_Ci(i64, operandToCompareOp(operand), ebx,0, egx));
        out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
        right.code.free();
    } else if(left.type == expression_field) {
        if(left.utype.field->isVar()) {
            equals(left, right);

            pushExpressionToRegister(right, out, egx);
            pushExpressionToRegister(left, out, ebx);
            out.code.push_i64(SET_Ci(i64, operandToCompareOp(operand), ebx,0, egx));
            out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
            right.code.free();
        }
        else {
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
        }
    } else {
        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() + "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
    }
}


Expression RuntimeEngine::parseEqualExpression(Ast* pAst) {
    Expression out, left, right;
    token_entity operand = pAst->getentity(0);

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary = <expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    left = parseIntermExpression(pAst->getsubast(0));
    right = parseExpression(pAst->getsubast(1));

    out.type = expression_var;
    switch(left.type) {
        case expression_var:
            if(operand.gettokentype() == ASSIGN) {
                assignValue(operand, out, left, right, pAst);
            } else {
                if(right.type == expression_var) {
                    assignNative(operand, out, left, right, pAst);
                }
                else if(right.type == expression_field) {
                    if(right.utype.field->isVar()) {
                        // add var
                        assignNative(operand, out, left, right, pAst);
                    } else if(right.utype.field->type == CLASS) {
                        addClass(operand, right.utype.field->klass, out, left, right, pAst);
                    } else {
                        // do nothing field unresolved
                    }
                } else if(right.type == expression_lclass) {
                    addClass(operand, left.utype.klass, out, left, right, pAst);
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                      "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
                }
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable");
            break;
        case expression_field:
            if(left.utype.field->isVar()) {
                // add var
                if(right.type == expression_null || operand.gettokentype() == ASSIGN) {
                    assignValue(operand, out, left, right, pAst);
                } else {
                    assignNative(operand, out, left, right, pAst);
                }
            } else if(left.utype.field->type == CLASS) {
                if(right.type == expression_null || operand.gettokentype() == ASSIGN) {
                    assignValue(operand, out, left, right, pAst);
                } else {
                    addClass(operand, left.utype.field->klass, out, left, right, pAst);
                }
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_lclass:
            if(right.type == expression_null || operand.gettokentype() == ASSIGN) {
                assignValue(operand, out, left, right, pAst);
            } else {
                addClass(operand, left.utype.field->klass, out, left, right, pAst);
            }
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_void:
            errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable/comparable");
            break;
        case expression_objectclass:
            if(right.type==expression_objectclass || right.type==expression_lclass
               || right.type==expression_field) {
                assignValue(operand, out, left, right, pAst);
            } else
                errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable/comparable. Did you forget to apply a cast? "
                                                                         "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
            break;
        case expression_string:
            if(operand.gettokentype() == ASSIGN) {
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            } else {

                constructNewString(left, right, operand, out, left.lnk);
            }
            break;
        default:
            break;
    }

    out.lnk = pAst;
    return out;
}

long long andExprs=0;
void RuntimeEngine::parseAndExpressionChain(Expression& out, Ast* pAst) {
    Expression leftExpr, rightExpr, peekExpr;
    int operandPtr = 0;
    bool firstEval=true;
    token_entity operand;
    List<token_entity> operands;
    operands.add(pAst->getentity(0));
    bool value=0;
    for(unsigned int i = 0; i < pAst->getsubastcount(); i++) {
        if(pAst->getsubast(i)->getentitycount() > 0 && isMathOp(pAst->getsubast(i)->getentity(0)))
            operands.add(pAst->getsubast(i)->getentity(0));
    }

    operandPtr=operands.size()-1;
    for(long int i = pAst->getsubastcount()-1; i >= 0; i--) {
        if(firstEval) {
            rightExpr = pAst->getsubast(i)->gettype() == ast_expression ? parseExpression(pAst->getsubast(i))
                                                                        : parseIntermExpression(pAst->getsubast(i));
            i--;
            firstEval= false;
        }
        leftExpr = pAst->getsubast(i)->gettype() == ast_expression ? parseExpression(pAst->getsubast(i)) : parseIntermExpression(pAst->getsubast(i));
        operand = operands.get(operandPtr--);

        out.boolExpressions.appendAll(rightExpr.boolExpressions);
        out.boolExpressions.appendAll(leftExpr.boolExpressions);

        out.type=expression_var;
        switch(leftExpr.type) {
            case expression_var:
                if(rightExpr.type == expression_var) {
                    if(leftExpr.literal && rightExpr.literal) {
                        if(operand == "&&") {
                            value=leftExpr.intValue&&rightExpr.intValue;
                            out.code.push_i64(SET_Di(i64, op_MOVI, value), ebx);
                        } else if(operand == "||") {
                            value=leftExpr.intValue||rightExpr.intValue;
                            out.code.push_i64(SET_Di(i64, op_MOVI, value), ebx);
                        } else if(operand == "&") {
                            out.code.push_i64(SET_Di(i64, op_MOVI, leftExpr.intValue&(int64_t)rightExpr.intValue), ebx);
                        } else if(operand == "|") {
                            out.code.push_i64(SET_Di(i64, op_MOVI, leftExpr.intValue|(int64_t)rightExpr.intValue), ebx);
                        } else if(operand == "^") {
                            out.code.push_i64(SET_Di(i64, op_MOVI, leftExpr.intValue^(int64_t)rightExpr.intValue), ebx);
                        }

                    } else {
                        evaluate:
                        if((leftExpr.literal || rightExpr.literal) && operand == "||") {
                            if(leftExpr.literal) {
                                out.code.push_i64(SET_Di(i64, op_MOVI, leftExpr.intValue==0), ebx);
                            } else {
                                out.code.push_i64(SET_Di(i64, op_MOVI, rightExpr.intValue==0), ebx);
                            }
                        } else {
                            // is left leftexpr a literal?
                            if(operand == "&&") {
                                pushExpressionToRegister(leftExpr, out, ebx);

                                if(andExprs==1) {
                                    out.code.push_i64(SET_Di(i64, op_SKNE, 8));
                                    out.boolExpressions.add(out.code.size()-1);
                                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                                } else {
                                    out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));
                                }

                                pushExpressionToRegister(rightExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                                out.code.push_i64(SET_Ci(i64, op_AND, ebx,0, ecx)); // the oder in which we eval each side dosent freakn matter!

                                if(andExprs==1) {
                                    out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));

                                    if((i-1) >= 0) {
                                        out.code.push_i64(SET_Di(i64, op_SKNE, 8));
                                        out.boolExpressions.add(out.code.size()-1);
                                    }
                                } else {
                                    out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                                }
                            } else if(operand == "||") {
                                pushExpressionToRegister(leftExpr, out, ebx);

                                if(andExprs==1) {
                                    out.code.push_i64(SET_Di(i64, op_SKNE, 8));
                                    out.boolExpressions.add(out.code.size()-1);
                                } else {
                                    out.code.push_i64(SET_Di(i64, op_SKPE, rightExpr.code.size()));
                                    pushExpressionToRegister(rightExpr, out, ebx);
                                    if(andExprs==1) {
                                        out.code.push_i64(SET_Di(i64, op_SKNE, 8));
                                        out.boolExpressions.add(out.code.size()-1);
                                    }
                                }
                            } else if(operand == "|") {
                                pushExpressionToRegister(leftExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));

                                pushExpressionToRegister(rightExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                                out.code.push_i64(SET_Ci(i64, op_OR, ecx,0, ebx));
                                out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                            } else if(operand == "&") {
                                pushExpressionToRegister(leftExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));

                                pushExpressionToRegister(rightExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                                out.code.push_i64(SET_Ci(i64, op_UAND, ecx,0, ebx));
                                out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                            } else if(operand == "^") {
                                pushExpressionToRegister(leftExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_RSTORE, ebx));

                                pushExpressionToRegister(rightExpr, out, ebx);
                                out.code.push_i64(SET_Di(i64, op_LOADVAL, ecx));
                                out.code.push_i64(SET_Ci(i64, op_UNOT, ecx,0, ebx));
                                out.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, cmt));
                            }
                        }

                        rightExpr.func=false;
                        rightExpr.literal = false;
                        rightExpr.code.free();
                        rightExpr.type=expression_var;
                    }

                }else if(rightExpr.type == expression_field) {
                    if(rightExpr.utype.field->isVar()) {
                        // add var
                        goto evaluate;
                    } else if(rightExpr.utype.field->type == CLASS) {
                        errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                          "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                    } else {
                        // do nothing field unresolved
                    }
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                      "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                }
                break;
            case expression_null:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                break;
            case expression_field:
                if(leftExpr.utype.field->isVar()) {
                    // add var
                    goto evaluate;
                } else if(leftExpr.utype.field->type == CLASS) {
                    addClass(operand, leftExpr.utype.field->klass, out, leftExpr, rightExpr, pAst);
                } else {
                    // do nothing field unresolved
                }
                break;
            case expression_native:
                errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + leftExpr.typeToString() + "`");
                break;
            case expression_lclass:
                addClass(operand, out.utype.klass, out, out, rightExpr, pAst);
                break;
            case expression_class:
                errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + leftExpr.typeToString() + "`");
                break;
            case expression_void:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`");
                break;
            case expression_objectclass:
                errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                  "` cannot be applied to expression of type `" + leftExpr.typeToString() + "` and `" + rightExpr.typeToString() + "`. Did you forget to apply a cast? "
                                                                          "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
                break;
            case expression_string:
                // TODO: construct new string(<string>) and use that to concatonate strings
                break;
            default:
                break;
        }
    }


    /*
     * So we dont missinterpret the value returned from the expr :)
     */
    out.func=rightExpr.func;
    out.literal=rightExpr.literal=false;
    if(--andExprs == 0) {
        for(unsigned int i = 0; i < out.boolExpressions.size(); i++) {
            long index=out.boolExpressions.get(i);
            int op = GET_OP(out.code.__asm64.get(index));

            out.code.__asm64.replace(index, SET_Di(i64, op, ((out.code.size()-1)-index)-1));
        }
    }
}

Expression RuntimeEngine::parseAndExpression(Ast* pAst) {
    Expression out, left, right;
    token_entity operand = pAst->getentity(0);
    andExprs++;

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary << <expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    parseAndExpressionChain(out, pAst);

    out.lnk = pAst;
    return out;
}

Expression RuntimeEngine::parseAssignExpression(Ast* pAst) {
    Expression out, left, right;
    token_entity operand = pAst->getentity(0);

    if(pAst->getsubastcount() == 1) {
        // cannot compute unary = <expression>
        errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + operand.gettoken() + "`");
        return Expression(pAst);
    }

    left = parseIntermExpression(pAst->getsubast(0));
    right = parseExpression(pAst->getsubast(1));

    out.type = expression_var;
    switch(left.type) {
        case expression_var:
            if(operand.gettokentype() == ASSIGN) {
                errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable");
            } else {
                if(right.type == expression_var) {
                    assignNative(operand, out, left, right, pAst);
                }
                else if(right.type == expression_field) {
                    if(right.utype.field->isVar()) {
                        // add var
                        assignValue(operand, out, left, right, pAst);
                    } else if(right.utype.field->type == CLASS) {
                        addClass(operand, right.utype.field->klass, out, left, right, pAst);
                    } else {
                        // do nothing field unresolved
                    }
                } else if(right.type == expression_lclass) {
                    addClass(operand, left.utype.klass, out, left, right, pAst);
                } else {
                    errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                                      "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
                }
            }
            break;
        case expression_null:
            errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable");
            break;
        case expression_field:
            if(left.utype.field->isVar()) {
                // add var
                assignValue(operand, out, left, right, pAst);
            } else if(left.utype.field->type == CLASS) {
                addClass(operand, left.utype.field->klass, out, left, right, pAst);
            } else {
                // do nothing field unresolved
            }
            break;
        case expression_native:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_lclass:
            addClass(operand, left.utype.klass, out, left, right, pAst);
            break;
        case expression_class:
            errors->newerror(UNEXPECTED_SYMBOL, pAst->line, pAst->col, " `" + left.typeToString() + "`");
            break;
        case expression_void:
            errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable");
            break;
        case expression_objectclass:
            errors->newerror(GENERIC, pAst->line, pAst->col, "expression is not assignable. Did you forget to apply a cast? "
                                                                     "i.e ((SomeClass)dynamic_class) " + operand.gettoken() + " <data>");
            break;
        case expression_string:
            errors->newerror(GENERIC, pAst->line,  pAst->col, "Binary operator `" + operand.gettoken() +
                                                              "` cannot be applied to expression of type `" + left.typeToString() + "` and `" + right.typeToString() + "`");
            break;
        default:
            break;
    }

    out.lnk = pAst;
    return out;
}


Expression RuntimeEngine::parseBinaryExpression(Ast* pAst) {
    switch(pAst->gettype()) {
        case ast_add_e:
            return parseAddExpression(pAst);
        case ast_mult_e:
            return parseMultExpression(pAst);
        case ast_shift_e:
            return parseShiftExpression(pAst);
        case ast_less_e:
            return parseLessExpression(pAst);
        case ast_equal_e:
            return parseEqualExpression(pAst);
        case ast_and_e:
            return parseAndExpression(pAst);
        case ast_assign_e:
            return parseAssignExpression(pAst);
        default:
            return Expression(pAst);
    }
}

Expression RuntimeEngine::parseQuesExpression(Ast* pAst) {
    Expression condition,condIfTrue, condIfFalse, expression;

    condition = parseIntermExpression(pAst->getsubast(0));
    condIfTrue = parseExpression(pAst->getsubast(1));
    condIfFalse = parseExpression(pAst->getsubast(2));
    expression = condIfTrue;

    expression.code.__asm64.free();
    pushExpressionToRegister(condition, expression, cmt);

    expression.code.push_i64(SET_Ci(i64, op_LOADPC_2, adx,0, (condIfTrue.code.size() + 3)));
    expression.code.push_i64(SET_Ei(i64, op_IFNE));

    expression.code.inject(expression.code.size(), condIfTrue.code);
    expression.code.push_i64(SET_Di(i64, op_SKIP, condIfFalse.code.size()));
    expression.code.inject(expression.code.size(), condIfFalse.code);


    if(equals(condIfTrue, condIfFalse)) {
        if(condIfTrue.type == expression_class) {
            errors->newerror(GENERIC, condIfTrue.lnk->line,  condIfTrue.lnk->col, "Class `" + condIfTrue.typeToString() + "` must be lvalue");
            return condIfTrue;
        }
    }

    return expression;
}

Expression RuntimeEngine::parseExpression(Ast *ast) {
    Expression expression;
    Ast *encap = ast->getSubAst(ast_expression)->getSubAst(0);

    switch (encap->getType()) {
        case ast_primary_expr:
            return parsePrimaryExpression(encap);
        case ast_self_e:
            return parseSelfExpression(encap);
        case ast_base_e:
            return parseBaseExpression(encap);
        case ast_null_e:
            return parseNullExpression(encap);
        case ast_new_e:
            return parseNewExpression(encap);
        case ast_sizeof_e:
            return parseSizeOfExpression(encap);
        case ast_post_inc_e:
            return parsePostInc(encap);
        case ast_arry_e:
            return parseArrayExpression(encap);
        case ast_cast_e:
            return parseCastExpression(encap);
        case ast_pre_inc_e:
            return parsePreInc(encap);
        case ast_paren_e:
            return parseParenExpression(encap);
        case ast_not_e:
            return parseNotExpression(encap);
        case ast_vect_e:
            errors->createNewError(GENERIC, ast->line, ast->col,
                             "unexpected vector array expression. Did you mean 'new type { <data>, .. }'?");
            expression.lnk = encap;
            return expression;
        case ast_add_e:
        case ast_mult_e:
        case ast_shift_e:
        case ast_less_e:
        case ast_equal_e:
        case ast_and_e:
        case ast_assign_e:
            return parseBinaryExpression(encap);
        case ast_ques_e:
            return parseQuesExpression(encap);
        default:
            stringstream err;
            err << ": unknown ast type: " << ast->gettype();
            errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
            return Expression(encap); // not an expression!
    }
}

Expression RuntimeEngine::parseValue(Ast *ast) {
    return parseExpression(ast);
}

void RuntimeEngine::analyzeVarDecl(Ast *ast) {
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=0;
    token_entity operand;

    parseAccessDecl(ast, modifiers, startpos);

    string name =  ast->getEntity(startpos).getToken();
    Field* field = scope->klass->getField(name);

    if(ast->hasSubAst(ast_value)) {
        Expression expression = parse_value(ast->getSubAst(ast_value)), out;
        Expression fieldExpr = fieldToExpression(ast, *field);
        fieldExpr.code.free();
        equals(fieldExpr, expression);
        operand=ast->getEntity(ast->getSubAstCount()-1);

        if(field->isStatic() && field->nativeInt() && !field->array && expression.literal) {
            // inline local static variables
            inline_map.add(keypair<string, double>(field->fullName, expression.intValue));
        } else {
            fieldExpr.code.__asm64.push_back(SET_Di(i64, op_MOVL, 0));
            fieldExpr.code.__asm64.push_back(SET_Di(i64, op_MOVN, field->vaddr));

            if(field->isObjectInMemory()) {
                if(operand == "=") {
                    if(field->type==field_class && !expression._new) {
                        initalizeNewClass(field->klass, out);
                        out.code.__asm64.push_back(SET_Di(i64, op_MOVL, 0));
                        out.code.push_i64(SET_Di(i64, op_MOVL, field->vaddr));
                        out.code.push_i64(SET_Ei(i64, op_POPREF));
                    }

                    assignValue(operand, out, fieldExpr, expression, ast);
                } else {
                    errors->createNewError(GENERIC, ast, " explicit call to operator `" + operand.getToken() + "` without initilization");
                }
            } else {
                assignValue(operand, out, fieldExpr, expression, ast);
            }

            if(field->isStatic()) {
                Method* main = getMainMethod(_current);
                if(main != NULL) {
                    main->code.inject(0, out.code); // inilize static variable at runtime start in main method
                }
            } else {
                /*
                 * We want to inject the value into all constructors
                 */
                for(unsigned int i = 0; i < scope->klass->constructorCount(); i++) {
                    Method* method = scope->klass->getConstructor(i);
                    method->code.inject(method->code.size(), out.code);
                }
            }

        }
    }

//
//    if(pAst->hasentity(COMMA)) {
//
//    }
    // TODO: check for value assignment and other vars.
    // if it dosent have a var decl init it to default value in injector (if not in method)
}

void RuntimeEngine::analyzeImportDecl(Ast *pAst) {
    string import = parseModuleName(pAst);
    if(import == currentModule) {
        createNewWarning(REDUNDANT_IMPORT, pAst->line, pAst->col, " '" + import + "'");
    } else {
        if(!modules.find(import)) {
            errors->createNewError(COULD_NOT_RESOLVE, pAst->line, pAst->col,
                             " `" + import + "` ");
        }
    }
}

void RuntimeEngine::createNewWarning(error_type error, int line, int col, string xcmnts) {
    if(c_options.warnings) {
        if(c_options.werrors){
            errors->createNewError(error, line, col, xcmnts);
        } else {
            errors->createNewError(error, line, col, xcmnts);
        }
    }
}

Method *RuntimeEngine::getMainMethod(Parser *p) {
    string starter_classname = "Runtime";

    ClassObject* StarterClass = getClass("internal", starter_classname);
    if(StarterClass != NULL) {
        List<Param> params;
        List<AccessModifier> modifiers;
        RuntimeNote note = RuntimeNote(p->sourcefile, p->getErrors()->getLine(1), 1, 0);
        params.add(Field(OBJECT, 0, "args", StarterClass, modifiers, note));
        params.get(0).field.isArray = true;

        Method* main = StarterClass->getFunction("__srt_init_" , params);

        if(main == NULL) {
            errors->createNewError(GENERIC, 1, 0, "could not locate main method '__srt_init_(object[])' in starter class");
        } else {
            if(!main->isStatic()) {
                errors->createNewError(GENERIC, 1, 0, "main method '__srt_init_(object[])' must be static");
            }
            if(!main->hasModifier(PUBLIC)) {
                errors->createNewError(GENERIC, 1, 0, "main method '__srt_init_(object[])' must be public");
            }

            RuntimeEngine::main = main;
        }
        return main;
    } else {
        errors->createNewError(GENERIC, 1, 0, "Could not find starter class '" + starter_classname + "' for application entry point.");
        return NULL;
    }
}

void RuntimeEngine::resolveAllMethods() {
    resolvedFields = true;
    /*
     * All fields have been processed, so we are good to fully parse utypes for method params
     */
    resolveAllFields();
}

/**
 * We need to know about all the classes & variables before we attempt
 * to try to resolve anything
 * @return
 */
bool RuntimeEngine::preprocess()
{
    bool success = true;

    for(unsigned long i = 0; i < parsers.size(); i++)
    {
        activeParser = parsers.get(i);
        errors = new ErrorManager(activeParser->lines, activeParser->sourcefile, true, c_options.aggressive_errors);

        currentModule = "$unknown";
        KeyPair<string, List<string>> resolveMap;
        List<string> imports;

        sourceFiles.addif(activeParser->sourcefile);
        addScope(Scope(GLOBAL_SCOPE, NULL));
        for(unsigned long i = 0; i < activeParser->treesize(); i++)
        {
            Ast *ast = activeParser->ast_at(i);
            SEMTEX_CHECK_ERRORS

            if(i == 0 && ast->getType() == ast_module_decl) {
                add_module(currentModule = parseModuleName(ast));
                imports.push_back(currentModule);
                continue;
            } else if(i == 0)
                errors->createNewError(GENERIC, ast->line, ast->col, "module declaration must be "
                        "first in every file");

            switch(ast->getType()) {
                case ast_class_decl:
                    parseClassDecl(ast);
                    break;
                case ast_import_decl:
                    imports.add(parseModuleName(ast));
                    break;
                case ast_module_decl: /* fail-safe */
                    errors->createNewError(GENERIC, ast->line, ast->col, "file module cannot be declared more than once");
                    break;
                default:
                    stringstream err;
                    err << ": unknown ast type: " << ast->getType();
                    errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
                    break;
            }
        }

        resolveMap.set(activeParser->sourcefile, imports);
        importMap.push_back(resolveMap);
        if(errors->hasErrors()){
            report:

            errorCount+= errors->getErrorCount();
            unfilteredErrorCount+= errors->getUnfilteredErrorCount();

            success = false;
            failedParsers.addif(activeParser->sourcefile);
            succeededParsers.removefirst(activeParser->sourcefile);
        } else {
            failedParsers.addif(activeParser->sourcefile);
            succeededParsers.removefirst(activeParser->sourcefile);
        }

        errors->free();
        delete (errors); this->errors = NULL;
        removeScope();
    }



    return success;
}

void RuntimeEngine::resolveAllFields() {
    for(unsigned long i = 0; i < parsers.size(); i++) {
        activeParser = parsers.get(i);
        errors = new ErrorManager(activeParser->lines, activeParser->sourcefile, true, c_options.aggressive_errors);
        currentModule = "$unknown";

        addScope(Scope(GLOBAL_SCOPE, NULL));
        for(int i = 0; i < activeParser->treesize(); i++) {
            Ast* ast = activeParser->ast_at(i);
            SEMTEX_CHECK_ERRORS

            if(i==0) {
                if(ast->getType() == ast_module_decl) {
                    add_module(currentModule = parseModuleName(ast));
                    continue;
                }
            }

            switch(ast->getType()) {
                case ast_class_decl:
                    resolveClassDecl(ast);
                    break;
                default:
                    /* ignore */
                    break;
            }
        }

        if(errors->hasErrors()){
            report:

            errorCount+= errors->getErrorCount();
            unfilteredErrorCount+= errors->getUnfilteredErrorCount();

            failedParsers.addif(activeParser->sourcefile);
            succeededParsers.removefirst(activeParser->sourcefile);
        } else {
            failedParsers.addif(activeParser->sourcefile);
            succeededParsers.removefirst(activeParser->sourcefile);
        }

        errors->free();
        delete (errors); this->errors = NULL;
        removeScope();
    }
}

ReferencePointer RuntimeEngine::parseReferencePtr(Ast *ast) {
    ast = ast->getSubAst(ast_refrence_pointer);
    bool hashfound = false, last, hash = ast->hasEntity(HASH);
    string id="";
    ReferencePointer ptr;

    for(long i = 0; i < ast->getEntityCount(); i++) {
        id = ast->getEntity(i).getToken();
        last = i + 1 >= ast->getEntityCount();

        if(id == ".")
            continue;
        else if(id == "#") {
            hashfound = true;
            continue;
        }

        if(hash && !hashfound && !last) {
            if(ptr.module == "")
                ptr.module =id;
            else
                ptr.module += "." + id;
        } else if(!last) {
            ptr.classHeiarchy.push_back(id);
        } else {
            ptr.referenceName = id;
        }
    }

    return ptr;
}

ClassObject* RuntimeEngine::tryClassResolve(string moduleName, string name) {
    ClassObject* klass = NULL;

    if((klass = getClass(moduleName, name)) == NULL) {
        for(unsigned int i = 0; i < importMap.size(); i++) {
            if(importMap.get(i).key == activeParser->sourcefile) {

                List<string>& lst = importMap.get(i).value;
                for(unsigned int x = 0; x < lst.size(); x++) {
                    if((klass = getClass(lst.get(i), name)) != NULL)
                        return klass;
                }

                break;
            }
        }
    }

    return klass;
}

ResolvedReference RuntimeEngine::resolveReferencePointer(ReferencePointer &ptr) {
    ResolvedReference reference;

    if(ptr.classHeiarchy.size() == 0) {
        ClassObject* klass = tryClassResolve(ptr.module, ptr.referenceName);
        if(klass == NULL) {
            reference.type = UNDEFINED;
            reference.referenceName = ptr.referenceName;
        } else {
            reference.type = CLASS;
            reference.klass = klass;
            reference.resolved = true;
        }
    } else {
        ClassObject* klass = tryClassResolve(ptr.module, ptr.classHeiarchy.get(0));
        if(klass == NULL) {
            reference.type = UNDEFINED;
            reference.referenceName = ptr.classHeiarchy.get(0);
        } else {
            ClassObject* childClass = NULL;
            string className;
            for(size_t i = 1; i < ptr.classHeiarchy.size(); i++) {
                className = ptr.classHeiarchy.get(i);

                if((childClass = klass->getChildClass(className)) == NULL) {
                    reference.type = UNDEFINED;
                    reference.referenceName = className;
                    return reference;
                } else {
                    klass = childClass;
                }

            }

            if(childClass != NULL) {
                if(childClass->getChildClass(ptr.referenceName) != NULL) {
                    reference.type = CLASS;
                    reference.klass = klass->getChildClass(ptr.referenceName);
                    reference.resolved = true;
                } else {
                    reference.type = UNDEFINED;
                    reference.referenceName = ptr.referenceName;
                }
            } else {
                if(klass->getChildClass(ptr.referenceName) != NULL) {
                    reference.type = CLASS;
                    reference.klass = klass->getChildClass(ptr.referenceName);
                    reference.resolved = true;
                } else {
                    reference.type = UNDEFINED;
                    reference.referenceName = ptr.referenceName;
                }
            }
        }
    }

    return reference;
}

bool RuntimeEngine::expectReferenceType(ResolvedReference refrence, FieldType expectedType, bool method, Ast *ast) {
    if(refrence.type == expectedType && refrence.isMethod==method)
        return true;

    errors->createNewError(EXPECTED_REFRENCE_OF_TYPE, ast->line, ast->col, " '" + (method ? "method" : ResolvedReference::typeToString(expectedType)) + "' instead of '" +
                                                                       refrence.typeToString() + "'");
    return false;
}

ClassObject* RuntimeEngine::resolveClassRefrence(Ast *ast, ReferencePointer &ptr) {
    ResolvedReference resolvedRefrence = resolveReferencePointer(ptr);
    ast = ast->getSubAst(ast_refrence_pointer);

    if(!resolvedRefrence.resolved) {
        errors->createNewError(COULD_NOT_RESOLVE, ast->line, ast->col, " `" + resolvedRefrence.referenceName + "` " +
                                                                   (ptr.module == "" ? "" : "in module {" + ptr.module + "} "));
    } else {

        if(expectReferenceType(resolvedRefrence, CLASS, false, ast)) {
            return resolvedRefrence.klass;
        }
    }

    return NULL;
}

ClassObject *RuntimeEngine::parseBaseClass(Ast *ast, int startpos) {
    Scope* scope = currentScope();
    ClassObject* klass=NULL;

    if(startpos >= ast->getEntityCount()) {
        return NULL;
    } else {
        ReferencePointer ptr = parseReferencePtr(ast);
        klass = resolveClassRefrence(ast, ptr);

        if(klass != NULL) {
            if((scope->klass->getHeadClass() != NULL && scope->klass->getHeadClass()->isCurcular(klass)) ||
               scope->klass->match(klass) || klass->match(scope->klass->getHeadClass())) {
                errors->createNewError(GENERIC, ast->getSubAst(0)->line, ast->getSubAst(0)->col,
                                 "cyclic dependency of class `" + ptr.referenceName + "` in parent class `" + scope->klass->getName() + "`");
            }
        }
    }

    return klass;
}

string Expression::typeToString() {
    switch(type) {
        case expression_string:
            return "var[]";
        case expression_unresolved:
            return "?";
        case expression_var:
            return string("var") + (utype.array ? "[]" : "");
        case expression_lclass:
            return utype.typeToString();
        case expression_native:
            return utype.typeToString();
        case expression_unknown:
            return "?";
        case expression_class:
            return utype.typeToString();
        case expression_void:
            return "void";
        case expression_objectclass:
            return "object";
        case expression_field:
            return utype.typeToString();
        case expression_null:
            return "null";
    }
    return utype.typeToString();
}

void Expression::operator=(Expression expression) {
    this->type=expression.type;
    this->newExpression=expression.newExpression;

    this->code.free();
    this->code.inject(0, expression.code);
    this->dot=expression.dot;
    this->func=expression.func;
    this->intValue=expression.intValue;
    this->literal=expression.literal;
    this->link = expression.link;
    this->utype  = expression.utype;
    this->value = expression.value;
    this->arrayElement = expression.arrayElement;
    this->boolExpressions.addAll(expression.boolExpressions);
}

void Expression::inject(Expression &expression) {
    this->code.inject(this->code.size(), expression.code);
}

ReferencePointer RuntimeEngine::parseTypeIdentifier(Ast *ast) {
    ast = ast->getSubAst(ast_type_identifier);

    if(ast->getSubAstCount() == 0) {
        ReferencePointer ptr;
        ptr.referenceName = ast->getEntity(0).getToken();
        return ptr;
    } else
        return parseReferencePtr(ast);
}

FieldType RuntimeEngine::tokenToNativeField(string entity) {
    if(entity == "var")
        return VAR;
    else if(entity == "object")
        return OBJECT;
    return UNDEFINED;
}

double RuntimeEngine::getInlinedFieldValue(Field* field) {
    for(unsigned int i = 0; i < inline_map.size(); i++) {
        if(inline_map.get(i).key==field->fullName)
            return inline_map.get(i).value;
    }

    return 0;
}


bool RuntimeEngine::isFieldInlined(Field* field) {
    if(!field->isStatic())
        return false;

    for(unsigned int i = 0; i < inline_map.size(); i++) {
        if(inline_map.get(i).key==field->fullName)
            return true;
    }

    return false;
}

int64_t RuntimeEngine::get_low_bytes(double var) {
    stringstream ss;
    ss.precision(16);
    ss << var;
    string num = ss.str(), result = "";
    int iter=0;
    for(unsigned int i = 0; i < num.size(); i++) {
        if(num.at(i) == '.') {
            for(unsigned int x = i+1; x < num.size(); x++) {
                if(iter++ > 16)
                    break;
                result += num.at(x);
            }
            return strtoll(result.c_str(), NULL, 0);
        }
    }
    return 0;
}

void RuntimeEngine::inlineVariableValue(Expression &expression, Field *field) {
    int64_t i64;
    double value = getInlinedFieldValue(field);
    expression.free();

    if(isDClassNumberEncodable(value))
        expression.code.push_i64(SET_Di(i64, op_MOVI, value), ebx);
    else {
        expression.code.push_i64(SET_Di(i64, op_MOVBI, ((int64_t)value)), abs(get_low_bytes(value)));
        expression.code.push_i64(SET_Ci(i64, op_MOVR, ebx,0, bmr));
    }

    expression.type=expression_var;
    expression.literal=true;
    expression.intValue=value;
}

bool RuntimeEngine::isDClassNumberEncodable(double var) { return ((int64_t )var > DA_MAX || (int64_t )var < DA_MIN) == false; }

void RuntimeEngine::resolveClassHeiarchy(ClassObject* klass, ReferencePointer& refrence, Expression& expression, Ast* pAst, bool requireStatic) {
    int64_t i64;
    string object_name = "";
    Field* field = NULL;
    ClassObject* k;
    bool lastRefrence = false;

    for(unsigned int i = 1; i < refrence.classHeiarchy.size()+1; i++) {
        if(i >= refrence.classHeiarchy.size()) {
            // field? if not then class?
            lastRefrence = true;
            object_name = refrence.referenceName;
        } else
            object_name = refrence.classHeiarchy.at(i);

        if((k = klass->getChildClass(object_name)) == NULL) {

            // field?
            if((field = klass->getField(object_name, true)) != NULL) {
                // is static?
                if(!lastRefrence && field->isArray) {
                    errors->createNewError(INVALID_ACCESS, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " field array");
                } else if(requireStatic && !field->isStatic() && field->type != UNDEFINED) {
                    errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, "static access on instance field `" + object_name + "`");
                }

                // for now we are just generating code for x.x.f not Main.x...thats static access
                expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));

                if(lastRefrence) {
                    expression.utype.type = CLASSFIELD;
                    expression.utype.field = field;
                    expression.type = expression_field;
                }

                switch(field->type) {
                    case UNDEFINED:
                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = object_name;
                        return;
                    case VAR:
                        if(lastRefrence){
                            if(isFieldInlined(field)) {
                                inlineVariableValue(expression, field);
                            }
                        }
                        else {
                            errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, "invalid access to non-class field `" + object_name + "`");
                            expression.utype.referenceName = object_name;
                            return;
                        }
                        break;
                    case CLASS:
                        klass = field->klass;
                        break;
                }
            } else {
                errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + object_name + "` " +
                        (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));
                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = object_name;
                expression.type = expression_unknown;
                return;
            }
        } else {
            if(field != NULL) {
                field = NULL;
                errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " ecpected class or module before `" + object_name + "` ");
            }
            expression.code.push_i64(SET_Di(i64, op_MOVG, klass->address));
            klass = k;

            if(lastRefrence) {
                expression.utype.type = CLASSFIELD;
                expression.utype.klass = klass;
                expression.type = expression_class;
            }
        }
    }
}

void RuntimeEngine::resolveFieldHeiarchy(Field* field, ReferencePointer& refrence, Expression& expression, Ast* pAst) {
    switch(field->type) {
        case UNDEFINED:
            return;
        case VAR:
            errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, "field `" + field->name + "` cannot be de-referenced");
            expression.utype.type = VAR;
            return;
        case CLASS:
            resolveClassHeiarchy(field->klass, refrence, expression, pAst, false);
            return;
    }
}

void RuntimeEngine::resolveSelfUtype(Scope* scope, ReferencePointer& reference, Expression& expression, Ast* ast) {
    if(reference.singleRefrence()) {
        ClassObject* klass=NULL;
        Field* field=NULL;

        if(scope->type == GLOBAL_SCOPE) {
            /* cannot get self from global refrence */
            errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line,
                             ast->getSubAst(ast_type_identifier)->col,
                             "cannot get object `" + reference.referenceName + "` from self at global scope");

            expression.utype.type = UNDEFINED;
            expression.utype.referenceName = reference.referenceName;
            expression.type = expression_unresolved;
        } else {
            if(scope->type == STATIC_BLOCK) {
                errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "cannot get object `"
                                                + reference.referenceName + "` from self in static context");
            }

            if((field = scope->klass->getField(reference.referenceName)) != NULL) {
                // field?
                expression.utype.type = CLASSFIELD;
                expression.utype.field = field;
                expression.type = expression_field;

                if(isFieldInlined(field)) {
                    inlineVariableValue(expression, field);
                } else {
                    expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                    expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));
                }
            } else {
                /* Un resolvable */
                errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + reference.referenceName + "` " +
                                                          (reference.module.empty() ? "" : "in module {" + reference.module + "} "));

                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = reference.referenceName;
                expression.type = expression_unresolved;
            }
        }
    } else if(reference.singleRefrenceModule()) {
        /* Un resolvable */
        errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " use of module {"
                                        + reference.module + "} in expression signifies global access of object");

        expression.utype.type = UNDEFINED;
        expression.utype.referenceName = reference.referenceName;
        expression.type = expression_unresolved;
    } else {
        /* field? or class? */
        ClassObject* klass=NULL;
        Field* field=NULL;
        string starter_name = reference.classHeiarchy.at(0);

        if(scope->type == GLOBAL_SCOPE) {
            /* cannot get self from global refrence */
            errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "cannot get object `"
                                            + starter_name + "` from instance at global scope");

            expression.utype.type = UNDEFINED;
            expression.utype.referenceName = reference.referenceName;
            expression.type = expression_unresolved;
        } else {
            if(reference.module != "") {
                /* Un resolvable */
                errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " use of module {" + reference.module + "} in expression signifies global access of object");

                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = reference.referenceName;
                expression.type = expression_unresolved;
                return;
            }

            if(scope->type == STATIC_BLOCK) {
                errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "cannot get object `" + starter_name + "` from self in static context");

                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = reference.referenceName;
                expression.type = expression_unresolved;
            } else {

                if((field = scope->klass->getField(starter_name)) != NULL) {
                    expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                    expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));

                    resolveFieldHeiarchy(field, reference, expression, ast);
                } else {
                    /* Un resolvable */
                    errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + reference.referenceName + "` " +
                                                              (reference.module == "" ? "" : "in module {" + reference.module + "} "));

                    expression.utype.type = UNDEFINED;
                    expression.utype.referenceName = reference.referenceName;
                    expression.type = expression_unresolved;
                }
            }
        }
    }
}

ResolvedReference RuntimeEngine::getBaseClassOrField(string name, ClassObject* start) {
    ClassObject* base;
    Field* field;
    ResolvedReference reference;

    for(;;) {
        base = start->getBaseClass();

        if(base == NULL) {
            // could not resolve
            return reference;
        }

        if(name == base->getName()) {
            reference.klass = base;
            reference.type = CLASS;
            return reference;
            // base class
        } else if((field = base->getField(name))) {
            reference.field = field;
            reference.type = CLASSFIELD;
            return reference;
        } else {
            start = base; // recursivley assign klass to new base
        }
    }
}

void RuntimeEngine::resolveBaseUtype(Scope* scope, ReferencePointer& reference, Expression& expression, Ast* ast) {
    ClassObject* klass = scope->klass, *base;
    Field* field;
    ResolvedReference ref;

    if(scope->klass->getBaseClass() == NULL) {
        errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "class `" + scope->klass->getFullName() + "` does not inherit a base class");
        expression.utype.type = UNDEFINED;
        expression.utype.referenceName = reference.toString();
        expression.type = expression_unresolved;
        return;
    }

    if(reference.singleRefrence()) {
        ref = getBaseClassOrField(reference.referenceName, klass);
        if(ref.type != UNDEFINED) {
            if(ref.type == CLASSFIELD) {
                // field!
                expression.utype.type = CLASSFIELD;
                expression.utype.field = ref.field;
                expression.type = expression_field;
            } else {
                if(scope->type == STATIC_BLOCK) {
                    errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "cannot get object `" + ref.klass->getName()
                                                                                                                                         + "` from instance in static context");
                }

                if(scope->klass->hasBaseClass(ref.klass)) {
                    // base class
                    expression.utype.type = CLASS;
                    expression.utype.klass = ref.klass;
                    expression.type = expression_class;

                    expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                } else {
                    // klass provided is not a base
                    errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "class `" + reference.referenceName + "`" +
                                                                                                                                     (reference.module == "" ? "" : " in module {" + reference.module + "}")
                                                                                                                                     + " is not a base class of class " + scope->klass->getName());
                    expression.utype.type = UNDEFINED;
                    expression.utype.referenceName = reference.toString();
                    expression.type = expression_unresolved;
                }
            }
        } else {
            errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + reference.referenceName + "` " +
                                                      (reference.module == "" ? "" : "in module {" + reference.module + "} "));
            expression.utype.type = UNDEFINED;
            expression.utype.referenceName = reference.toString();
            expression.type = expression_unresolved;
        }
    } else if(reference.singleRefrenceModule()) {
        base = getClass(reference.module, reference.referenceName);

        if(base != NULL) {
            if(scope->klass->hasBaseClass(base)) {
                // base class

                expression.utype.type = CLASS;
                expression.utype.klass = base;
                expression.type = expression_class;
                expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                return;
            } else {
                // klass provided is not a base
                errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "class `" + reference.referenceName + "`" +
                                                                                                                                 (reference.module == "" ? "" : " in module {" + reference.module + "}")
                                                                                                                                 + " is not a base class of class " + scope->klass->getName());
                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = reference.toString();
                expression.type = expression_unresolved;
            }
        }

        /* Un resolvable */

        errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + reference.referenceName + "` " +
                                                  (reference.module == "" ? "" : "in module {" + reference.module + "} "));
        expression.utype.type = UNDEFINED;
        expression.utype.referenceName = reference.referenceName;
        expression.type = expression_unresolved;
        return;
    } else {
        base = klass->getBaseClass();
        string starter_name = reference.classHeiarchy.at(0);

        if(base != NULL) {

            if(reference.module != "") {
                // first must be class
                if((klass = getClass(reference.module, starter_name)) != NULL) {
                    if(scope->klass->hasBaseClass(klass)) {
                        expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                        resolveClassHeiarchy(klass, reference, expression, ast);
                    } else {
                        // klass provided is not a base
                        errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "class `" + starter_name + "`" +
                                                                                                                                         (reference.module == "" ? "" : " in module {" + reference.module + "}")
                                                                                                                                         + " is not a base class of class " + scope->klass->getName());
                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = reference.toString();
                        expression.type = expression_unresolved;
                    }
                    return;
                } else {
                    errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                                                              (reference.module == "" ? "" : "in module {" + reference.module + "} "));
                    expression.utype.type = UNDEFINED;
                    expression.utype.referenceName = reference.toString();
                    expression.type = expression_unresolved;
                }
            } else {
                ref = getBaseClassOrField(starter_name, klass);
                if(ref.type != UNDEFINED) {
                    if(ref.type == CLASSFIELD) {
                        expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                        expression.code.push_i64(SET_Di(i64, op_MOVN, ref.field->address)); // gain access to field in object

                        resolveFieldHeiarchy(ref.field, reference, expression, ast);
                    } else {
                        expression.code.push_i64(SET_Di(i64, op_MOVL, 0));

                        resolveClassHeiarchy(ref.klass, reference, expression, ast);
                    }
                } else {
                    if((klass = getClass(reference.module, starter_name)) != NULL) {
                        if(scope->klass->hasBaseClass(klass)) {
                            expression.code.push_i64(SET_Di(i64, op_MOVL, 0));

                            resolveClassHeiarchy(klass, reference, expression, ast);
                        } else {
                            errors->createNewError(GENERIC, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, "class `" + starter_name + "`" +
                                                                                                                                             (reference.module == "" ? "" : " in module {" + reference.module + "}")
                                                                                                                                             + " is not a base class of class " + scope->klass->getName());
                            expression.utype.type = UNDEFINED;
                            expression.utype.referenceName = reference.toString();
                            expression.type = expression_unresolved;
                        }
                    } else {
                        errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                                                                  (reference.module == "" ? "" : "in module {" + reference.module + "} "));
                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = reference.toString();
                        expression.type = expression_unresolved;
                    }
                }
            }
        }else {
            errors->createNewError(COULD_NOT_RESOLVE, ast->getSubAst(ast_type_identifier)->line, ast->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                                                      (reference.module == "" ? "" : "in module {" + reference.module + "} "));
            expression.utype.type = UNDEFINED;
            expression.utype.referenceName = reference.toString();
            expression.type = expression_unresolved;
        }
    }
}

void RuntimeEngine::resolveUtype(ReferencePointer& refrence, Expression& expression, Ast* pAst) {
    Scope* scope = currentScope();
    int64_t i64;

    expression.link = pAst;
    if(scope->self) {
        resolveSelfUtype(scope, refrence, expression, pAst);
    } else if(scope->base) {
        resolveBaseUtype(scope, refrence, expression, pAst);
    } else {
        if(refrence.singleRefrence()) {
            ClassObject* klass=NULL;
            Field* field=NULL;

            if(scope->type == GLOBAL_SCOPE) {

                if((klass = getClass(refrence.module, refrence.referenceName)) != NULL) {
                    expression.utype.type = CLASS;
                    expression.utype.klass = klass;
                    expression.type = expression_class;
                } else {
                    /* Un resolvable */
                    errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line,
                                           pAst->getSubAst(ast_type_identifier)->col, " `" + refrence.referenceName + "` " +
                                                                                      (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));

                    expression.utype.type = UNDEFINED;
                    expression.utype.referenceName = refrence.referenceName;
                    expression.type = expression_unresolved;
                }
            } else {

                // scope_class? | scope_instance_block? | scope_static_block?
                if(scope->type != CLASS_SCOPE && scope->getLocalField(refrence.referenceName) != NULL) {
                    field = &scope->getLocalField(refrence.referenceName)->value;
                    expression.utype.type = CLASSFIELD;
                    expression.utype.field = field;
                    expression.type = expression_field;

                    if(field->isVar()) {
                        if(field->isObjectInMemory()) {
                            expression.code.push_i64(SET_Di(i64, op_MOVL, field->address));
                        } else
                            expression.code.push_i64(SET_Ci(i64, op_LOADL, ebx, 0, field->address));
                    }
                    else
                        expression.code.push_i64(SET_Di(i64, op_MOVL, field->address));
                }
                else if((field = scope->klass->getField(refrence.referenceName, true)) != NULL) {
                    // field?
                    if(scope->type == STATIC_BLOCK) {
                        errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col,
                                         "cannot get object `" + refrence.referenceName + "` from self in static context");
                    }

                    expression.utype.type = CLASSFIELD;
                    expression.utype.field = field;
                    expression.type = expression_field;

                    if(isFieldInlined(field)) {
                        inlineVariableValue(expression, field);
                    } else {
                        expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                        expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));
                    }
                } else {
                    if((klass = getClass(refrence.module, refrence.referenceName)) != NULL) {
                        // global class ?
                        expression.utype.type = CLASS;
                        expression.utype.klass = klass;
                        expression.type = expression_class;
                    } else {
                        /* Un resolvable */
                        errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + refrence.referenceName + "` " +
                                                                                  (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));

                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = refrence.referenceName;
                        expression.type = expression_unresolved;
                    }
                }
            }
        } else if(refrence.singleRefrenceModule()){
            /* Must be a class i.e module#globalClass */
            ClassObject* klass=NULL;

            // in this case we ignore scope
            if((klass = getClass(refrence.module, refrence.referenceName)) != NULL) {
                expression.utype.type = CLASS;
                expression.utype.klass = klass;
                expression.type = expression_class;
            } else {
                /* Un resolvable */
                errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + refrence.referenceName + "` " +
                        (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));

                expression.utype.type = UNDEFINED;
                expression.utype.referenceName = refrence.referenceName;
                expression.type = expression_unresolved;
            }
        } else {
            /* field? or class? */
            ClassObject* klass=NULL;
            Field* field=NULL;
            string starter_name = refrence.classHeiarchy.at(0);

            if(scope->type == GLOBAL_SCOPE) {

                // class?
                if((klass = getClass(refrence.module, starter_name)) != NULL) {
                    resolveClassHeiarchy(klass, refrence, expression, pAst);
                    return;
                } else {
                    /* un resolvable */
                    errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                            (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));
                    expression.utype.type = UNDEFINED;
                    expression.utype.referenceName = refrence.toString();
                    expression.type = expression_unresolved;

                }
            } else {

                // scope_class? | scope_instance_block? | scope_static_block?
                if(refrence.module != "") {
                    if((klass = getClass(refrence.module, starter_name)) != NULL) {
                        resolveClassHeiarchy(klass, refrence, expression, pAst);
                        return;
                    } else {
                        errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                                (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));
                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = refrence.toString();
                        expression.type = expression_unresolved;
                        return;
                    }
                }

                if(scope->type != CLASS_SCOPE && scope->getLocalField(starter_name) != NULL) {
                    field = &scope->getLocalField(starter_name)->value;

                    /**
                     * You cannot access a var in this manner because it
                     * is not a data structure
                     */
                    if(field->isVar()) {
                        errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col,
                                         "field `" + starter_name + "` cannot be de-referenced");
                    }
                    else
                        expression.code.push_i64(SET_Di(i64, op_MOVL, field->address));
                    resolveFieldHeiarchy(field, refrence, expression, pAst);
                    return;
                }
                else if((field = scope->klass->getField(starter_name, true)) != NULL) {
                    if(scope->type == STATIC_BLOCK) {
                        errors->createNewError(GENERIC, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col,
                                               "cannot get object `" + starter_name + "` from self in static context");
                    }

                    expression.code.push_i64(SET_Di(i64, op_MOVL, 0));
                    expression.code.push_i64(SET_Di(i64, op_MOVN, field->address));
                    resolveFieldHeiarchy(field, refrence, expression, pAst);
                    return;
                } else {
                    if((klass = getClass(refrence.module, starter_name)) != NULL) {
                        resolveClassHeiarchy(klass, refrence, expression, pAst);
                        return;
                    } else {
                        errors->createNewError(COULD_NOT_RESOLVE, pAst->getSubAst(ast_type_identifier)->line, pAst->getSubAst(ast_type_identifier)->col, " `" + starter_name + "` " +
                                (refrence.module == "" ? "" : "in module {" + refrence.module + "} "));
                        expression.utype.type = UNDEFINED;
                        expression.utype.referenceName = refrence.toString();
                        expression.type = expression_unresolved;
                    }
                }
            }
        }
    }
}

Expression RuntimeEngine::parseUtype(Ast* ast) {
    ast = ast->getSubAst(ast_utype);

    ReferencePointer ptr=parseTypeIdentifier(ast);
    Expression expression;

    if(ptr.singleRefrence() && Parser::isnative_type(ptr.referenceName)) {
        expression.utype.type = tokenToNativeField(ptr.referenceName);
        expression.type = expression_native;
        expression.utype.referenceName = ptr.toString();

        if(ast->hasEntity(LEFTBRACE) && ast->hasEntity(RIGHTBRACE)) {
            expression.utype.array = true;
        }

        expression.link = ast;
        ptr.free();
        return expression;
    }

    resolveUtype(ptr, expression, ast);

    if(ast->hasEntity(LEFTBRACE) && ast->hasEntity(RIGHTBRACE)) {
        expression.utype.array = true;
    }

    expression.link = ast;
    expression.utype.referenceName = ptr.toString();
    ptr.free();
    return expression;
}

void RuntimeEngine::resolveVarDecl(Ast* ast) {
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=0;

    parseAccessDecl(ast, modifiers, startpos);
    string name =  ast->getEntity(startpos).getToken();
    Field* field = scope->klass->getField(name);
    Expression expression = parseUtype(ast);
    if(expression.utype.type == CLASS) {
        field->klass = expression.utype.klass;
        field->type = CLASS;
    } else if(expression.utype.type == VAR) {
        field->type = VAR;
    } else {
        field->type = UNDEFINED;
    }

    field->isArray = expression.utype.array;
    field->address = scope->klass->getFieldIndex(name);
}

int RuntimeEngine::parseMethodAccessSpecifiers(List<AccessModifier> &modifiers) {
    for(long i = 0; i < modifiers.size(); i++) {
        AccessModifier modifier = modifiers.get(i);
        if(modifier == mCONST || modifier == mUNDEFINED)
            return i;
    }

    if(modifiers.get(0) <= PROTECTED) {
        if(modifiers.size() > 3)
            return (int)(modifiers.size() - 1);
        else if(modifiers.size() == 2) {
            if(modifiers.get(1) != STATIC && modifiers.get(1) != OVERRIDE)
                return 1;
        }
        else if(modifiers.size() == 3) {
            if(modifiers.get(1) != STATIC)
                return 1;
            if(modifiers.get(2) != OVERRIDE)
                return 2;
        }
    }
    else if(modifiers.get(0) == STATIC) {
        if(modifiers.size() > 2)
            return (int)(modifiers.size() - 1);
        else if(modifiers.size() == 2 && modifiers.get(1) != OVERRIDE)
            return 1;
    }
    else if(modifiers.get(0) == OVERRIDE) {
        if(modifiers.size() != 1)
            return (int)(modifiers.size() - 1);
    }
    return -1;
}

void RuntimeEngine::parseMethodAccessModifiers(List<AccessModifier> &modifiers, Ast *pAst) {
    if(modifiers.size() > 3)
        this->errors->createNewError(GENERIC, pAst->line, pAst->col, "too many access specifiers");
    else {
        int m= parseMethodAccessSpecifiers(modifiers);
        switch(m) {
            case -1:
                break;
            default:
                this->errors->createNewError(INVALID_ACCESS_SPECIFIER, pAst->getEntity(m).getLine(),
                                       pAst->getEntity(m).getColumn(), " `" + pAst->getEntity(m).getToken() + "`");
                break;
        }
    }

    if(!modifiers.find(PUBLIC) && !modifiers.find(PRIVATE)
       && modifiers.find(PROTECTED)) {
        modifiers.add(PUBLIC);
    }
}

bool RuntimeEngine::containsParam(List<Param> params, string param_name) {
    for(unsigned int i = 0; i < params.size(); i++) {
        if(params.get(i).field.name == param_name)
            return true;
    }
    return false;
}

Field RuntimeEngine::fieldMapToField(string param_name, ResolvedReference& utype, Ast* pAst) {
    Field field;

    if(utype.type == CLASSFIELD) {
        errors->createNewError(COULD_NOT_RESOLVE, utype.field->note.getLine(), utype.field->note.getCol(), " `" + utype.field->name + "`");
        field.type = UNDEFINED;
        field.note = utype.field->note;
        field.modifiers.addAll(utype.field->modifiers);
    } else if(utype.type == CLASS) {
        field.type = CLASS;
        field.note = utype.klass->note;
        field.klass = utype.klass;
        field.modifiers.add(utype.klass->getAccessModifier());
    } else if(utype.type == VAR) {
        field.type = VAR;
        field.modifiers.add(PUBLIC);
    }
    else {
        field.type = UNDEFINED;
    }

    field.fullName = param_name;
    field.address = uniqueSerialId++;
    field.name = param_name;
    field.isArray = utype.array;
    field.note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(pAst->line), pAst->line, pAst->col);

    if(utype.type == CLASSFIELD)
        return *utype.field;

    return field;
}

void RuntimeEngine::parseMethodParams(List<Param>& params, KeyPair<List<string>, List<ResolvedReference>> fields, Ast* pAst) {
    for(unsigned int i = 0; i < fields.key.size(); i++) {
        if(containsParam(params, fields.key.get(i))) {
            errors->createNewError(SYMBOL_ALREADY_DEFINED, pAst->line, pAst->col, "symbol `" + fields.key.get(i) + "` already defined in the scope");
        } else
            params.add(Param(fieldMapToField(fields.key.get(i), fields.value.get(i), pAst)));
    }
}

KeyPair<string, ResolvedReference> RuntimeEngine::parseUtypeArg(Ast* ast) {
    KeyPair<string, ResolvedReference> utype_arg;
    utype_arg.value = parseUtype(ast).utype;
    if(ast->getEntityCount() != 0)
        utype_arg.key = ast->getEntity(0).getToken();
    else
        utype_arg.key = "";

    return utype_arg;
}

KeyPair<List<string>, List<ResolvedReference>> RuntimeEngine::parseUtypeArgList(Ast* ast) {
    KeyPair<List<string>, List<ResolvedReference>> utype_argmap;
    KeyPair<string, ResolvedReference> utype_arg;
    utype_argmap.key.init();
    utype_argmap.value.init();

    for(unsigned int i = 0; i < ast->getSubAstCount(); i++) {
        utype_arg = parseUtypeArg(ast->getSubAst(i));
        utype_argmap.key.push_back(utype_arg.key);
        utype_argmap.value.push_back(utype_arg.value);
    }

    return utype_argmap;
}

void RuntimeEngine::parseMethodReturnType(Expression& expression, Method& method) {
    method.array = expression.utype.array;
    if(expression.type == expression_class) {
        method.type = CLASS;
        method.klass = expression.utype.klass;
    } else if(expression.type == expression_native) {
        method.type = expression.utype.type;
    } else {
        method.type = UNDEFINED;
        errors->createNewError(GENERIC, expression.link->line, expression.link->col, "expected class or native type for method's return value");
    }
}

void RuntimeEngine::resolveMethodDecl(Ast* ast) {
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=1;

    if(parseAccessDecl(ast, modifiers, startpos)){
        parseMethodAccessModifiers(modifiers, ast);
    } else {
        modifiers.add(PUBLIC);
    }

    List<Param> params;
    string name =  ast->getEntity(startpos).getToken();
    parseMethodParams(params, parseUtypeArgList(ast->getSubAst(ast_utype_arg_list)), ast->getSubAst(ast_utype_arg_list));

    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                   ast->line, ast->col);

    Expression utype;
    Method method = Method(name, currentModule, scope->klass, params, modifiers, NULL, note, sourceFiles.indexof(activeParser->sourcefile));
    if(ast->hasSubAst(ast_method_return_type)) {
        utype = parseUtype(ast->getSubAst(ast_method_return_type));
        parseMethodReturnType(utype, method);
    } else
        method.type = TYPEVOID;

    method.address = methods++;
    if(!scope->klass->addFunction(method)) {
        this->errors->createNewError(PREVIOUSLY_DEFINED, ast->line, ast->col,
                               "function `" + name + "` is already defined in the scope");
        printNote(scope->klass->getFunction(name, params)->note, "function `" + name + "` previously defined here");
    }
}

Operator RuntimeEngine::stringToOp(string op) {
    if(op=="+")
        return oper_PLUS;
    if(op=="-")
        return oper_MINUS;
    if(op=="*")
        return oper_MULT;
    if(op=="/")
        return oper_DIV;
    if(op=="%")
        return oper_MOD;
    if(op=="++")
        return oper_INC;
    if(op=="--")
        return oper_DEC;
    if(op=="=")
        return oper_EQUALS;
    if(op=="==")
        return oper_EQUALS_EQ;
    if(op=="+=")
        return oper_PLUS_EQ;
    if(op=="-=")
        return oper_MIN_EQ;
    if(op=="*=")
        return oper_MULT_EQ;
    if(op=="/=")
        return oper_DIV_EQ;
    if(op=="&=")
        return oper_AND_EQ;
    if(op=="|=")
        return oper_OR_EQ;
    if(op=="!=")
        return oper_NOT_EQ;
    if(op=="%=")
        return oper_MOD_EQ;
    if(op=="!")
        return oper_NOT;
    if(op=="<<")
        return oper_SHL;
    if(op==">>")
        return oper_SHR;
    if(op=="<")
        return oper_LESSTHAN;
    if(op==">")
        return oper_GREATERTHAN;
    if(op=="<=")
        return oper_LTEQ;
    if(op==">=")
        return oper_GTEQ;
    return oper_UNDEFINED;
}

void RuntimeEngine::resolveOperatorDecl(Ast* ast) {
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=2;

    if(parseAccessDecl(ast, modifiers, startpos)){
        parseMethodAccessModifiers(modifiers, ast);
    } else {
        modifiers.add(PUBLIC);
    }

    List<Param> params;
    string op =  ast->getEntity(startpos).getToken();
    parseMethodParams(params, parseUtypeArgList(ast->getSubAst(ast_utype_arg_list)), ast->getSubAst(ast_utype_arg_list));

    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                   ast->line, ast->col);

    Expression utype;
    OperatorOverload operatorOverload = OperatorOverload(note, scope->klass, params, modifiers, NULL, stringToOp(op), sourceFiles.indexof(activeParser->sourcefile), op);
    if(ast->hasSubAst(ast_method_return_type)) {
        utype = parseUtype(ast->getSubAst(ast_method_return_type)->getSubAst(ast_utype));
        parseMethodReturnType(utype, operatorOverload);
    } else
        operatorOverload.type = TYPEVOID;

    operatorOverload.address = methods++;

    if(!scope->klass->addOperatorOverload(operatorOverload)) {
        this->errors->createNewError(PREVIOUSLY_DEFINED, ast->line, ast->col,
                               "function `" + op + "` is already defined in the scope");
        printNote(scope->klass->getOverload(stringToOp(op), params)->note, "function `" + op + "` previously defined here");
    }
}

void RuntimeEngine::parsConstructorAccessModifiers(List<AccessModifier> &modifiers, Ast * ast) {
    if(modifiers.size() > 1)
        this->errors->createNewError(GENERIC, ast->line, ast->col, "too many access specifiers");
    else {
        AccessModifier mod = modifiers.get(0);

        if(mod != PUBLIC && mod != PRIVATE && mod != PROTECTED)
            this->errors->createNewError(INVALID_ACCESS_SPECIFIER, ast->line, ast->col,
                                   " `" + ast->getEntity(0).getToken() + "`");
    }
}

void RuntimeEngine::resolveConstructorDecl(Ast* ast) {
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=0;


    if(parseAccessDecl(ast, modifiers, startpos)){
        parsConstructorAccessModifiers(modifiers, ast);
    } else {
        modifiers.add(PUBLIC);
    }

    List<Param> params;
    string name = ast->getEntity(startpos).getToken();

    if(name == scope->klass->getName()) {
        parseMethodParams(params, parseUtypeArgList(ast->getSubAst(ast_utype_arg_list)), ast->getSubAst(ast_utype_arg_list));
        RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                       ast->line, ast->col);

        Method method = Method(name, currentModule, scope->klass, params, modifiers, NULL, note, sourceFiles.indexof(activeParser->sourcefile));
        method.type = TYPEVOID;
        method.isConstructor=true;

        method.address = methods++;
        if(!scope->klass->addConstructor(method)) {
            this->errors->createNewError(PREVIOUSLY_DEFINED, ast->line, ast->col,
                                   "constructor `" + name + "` is already defined in the scope");
            printNote(scope->klass->getConstructor(params)->note, "constructor `" + name + "` previously defined here");
        }
    } else
        this->errors->createNewError(PREVIOUSLY_DEFINED, ast->line, ast->col,
                               "constructor `" + name + "` must be the same name as its parent");
}

void RuntimeEngine::addDefaultConstructor(ClassObject* klass, Ast* ast) {
    List<Param> emptyParams;
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    modifiers.add(PUBLIC);

    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                   ast->line, ast->col);

    if(klass->getConstructor(emptyParams, false) == NULL) {
        Method method = Method(klass->getName(), currentModule, scope->klass, emptyParams, modifiers, NULL, note, sourceFiles.indexof(activeParser->sourcefile));

        method.isConstructor=true;
        method.address = methods++;
        klass->addConstructor(method);
    }
}

void RuntimeEngine::resolveClassDecl(Ast* ast) {
    Scope* scope = currentScope();
    Ast* block = ast->getSubAst(ast_block), *trunk;
    List<AccessModifier> modifiers;
    ClassObject* klass;
    int startpos=1;

    parseAccessDecl(ast, modifiers, startpos);
    string name =  ast->getEntity(startpos).getToken();

    if(scope->type == GLOBAL_SCOPE) {
        klass = getClass(currentModule, name);
        klass->setFullName(currentModule + "#" + name);
    }
    else {
        klass = scope->klass->getChildClass(name);
        klass->setFullName(scope->klass->getFullName() + "." + name);
    }

    if(resolvedFields)
        klass->address = classSize++;

    addScope(Scope(CLASS_SCOPE, klass));
    klass->setBaseClass(parseBaseClass(ast, ++startpos));
    for(long i = 0; i < block->getSubAstCount(); i++) {
        trunk = block->getSubAst(i);
        CHECK_ERRORS

        switch(trunk->getType()) {
            case ast_class_decl:
                resolveClassDecl(trunk);
                break;
            case ast_var_decl:
                if(!resolvedFields)
                    resolveVarDecl(trunk);
                break;
            case ast_method_decl:
                if(resolvedFields)
                    resolveMethodDecl(trunk);
                break;
            case ast_operator_decl:
                if(resolvedFields)
                    resolveOperatorDecl(trunk);
                break;
            case ast_construct_decl:
                if(resolvedFields)
                    resolveConstructorDecl(trunk);
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << trunk->getType();
                errors->createNewError(INTERNAL_ERROR, trunk->line, trunk->col, err.str());
                break;
        }
    }

    if(resolvedFields)
        addDefaultConstructor(klass, ast);
    removeScope();
}

Scope* RuntimeEngine::addScope(Scope scope) {
    scopeMap.push_back(scope);
    return currentScope();
}

void RuntimeEngine::add_module(string name) {
    if(!module_exists(name)) {
        modules.push_back(name);
    }
}

bool RuntimeEngine::module_exists(string name) {
    for(unsigned long i = 0; i < modules.size(); i++) {
        if(modules.get(i) == name)
            return true;
    }

    return false;
}

string RuntimeEngine::parseModuleName(Ast *ast) {
    if(ast == NULL) return "";
    ast = ast->getSubAst(0); // module_list

    stringstream str;
    for(long i = 0; i < ast->getEntityCount(); i++) {
        str << ast->getEntity(i).getToken();
    }
    return str.str();
}

bool RuntimeEngine::isTokenAccessDecl(token_entity token) {
    return
            token.getId() == IDENTIFIER && token.getToken() == "protected" ||
            token.getId() == IDENTIFIER && token.getToken() == "private" ||
            token.getId() == IDENTIFIER && token.getToken() == "static" ||
            token.getId() == IDENTIFIER && token.getToken() == "const" ||
            token.getId() == IDENTIFIER && token.getToken() == "override" ||
            token.getId() == IDENTIFIER && token.getToken() == "public";
}

AccessModifier RuntimeEngine::entityToModifier(token_entity entity) {
    if(entity.getToken() == "public")
        return PUBLIC;
    else if(entity.getToken() == "private")
        return PRIVATE;
    else if(entity.getToken() == "protected")
        return PROTECTED;
    else if(entity.getToken() == "const")
        return mCONST;
    else if(entity.getToken() == "static")
        return STATIC;
    else if(entity.getToken() == "override")
        return OVERRIDE;
    return mUNDEFINED;
}


list<AccessModifier> RuntimeEngine::parseAccessModifier(Ast *ast) {
    int iter=0;
    list<AccessModifier> modifiers;

    do {
        modifiers.push_back(entityToModifier(ast->getEntity(iter++)));
    }while(isTokenAccessDecl(ast->getEntity(iter)));

    return modifiers;
}

bool RuntimeEngine::parseAccessDecl(Ast *ast, List<AccessModifier> &modifiers, int &startpos) {
    if(ast == NULL) return false;

    if(isTokenAccessDecl(ast->getEntity(0))) {
        list<AccessModifier> mods = parseAccessModifier(ast);
        modifiers.addAll(mods);
        startpos+=modifiers.size();
        return true;
    }
    return false;
}

void RuntimeEngine::parseClassAccessModifiers(List<AccessModifier> &modifiers, Ast* ast) {
    if(modifiers.size() > 1)
        this->errors->createNewError(GENERIC, ast->line, ast->col, "class objects only allows for a single access "
                "specifier (public, private, protected)");
    else {
        AccessModifier mod = modifiers.at(0);

        if(mod != PUBLIC && mod != PRIVATE && mod != PROTECTED)
            this->errors->createNewError(INVALID_ACCESS_SPECIFIER, ast->line, ast->col,
                                   " `" + ast->getEntity(0).getToken() + "`");
    }
}

bool RuntimeEngine::classExists(string module, string name) {
    ClassObject* klass = NULL;
    for(unsigned int i = 0; i < classes.size(); i++) {
        klass = &classes.get(i);
        if(klass->getName() == name) {
            if(module != "")
                return klass->getModuleName() == module;
            return true;
        }
    }

    return false;
}

bool RuntimeEngine::addClass(ClassObject klass) {
    if(!classExists(klass.getModuleName(), klass.getName())) {
        classes.add(klass);
        return true;
    }
    return false;
}

void RuntimeEngine::printNote(RuntimeNote& note, string msg) {
    if(lastNoteMsg != msg && lastNote.getLine() != note.getLine()
            && !noteMessages.find(msg))
    {
        cout << note.getNote(msg);
        lastNoteMsg = msg;
        noteMessages.push_back(msg);
    }
}

ClassObject *RuntimeEngine::getClass(string module, string name) {
    ClassObject* klass = NULL;
    for(unsigned int i = 0; i < classes.size(); i++) {
        klass = &classes.get(i);
        if(klass->getName() == name) {
            if(module != "" && klass->getModuleName() == module)
                return klass;
            else if(module == "")
                return klass;
        }
    }

    return NULL;
}

ClassObject *RuntimeEngine::addGlobalClassObject(string name, List<AccessModifier>& modifiers, Ast *pAst) {
    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(pAst->line),
                                   pAst->line, pAst->col);

    if(!this->addClass(ClassObject(name, currentModule, this->uniqueSerialId++,modifiers.get(0), note))){

        this->errors->createNewError(PREVIOUSLY_DEFINED, pAst->line, pAst->col, "class `" + name +
                                                                          "` is already defined in module {" + currentModule + "}");

        printNote(this->getClass(currentModule, name)->note, "class `" + name + "` previously defined here");
        return getClass(currentModule, name);
    } else
        return getClass(currentModule, name);
}

ClassObject *RuntimeEngine::addChildClassObject(string name, List<AccessModifier>& modifiers, Ast *ast, ClassObject* super) {
    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                   ast->line, ast->col);

    if(!super->addChildClass(ClassObject(name,
                                         currentModule, this->uniqueSerialId++, modifiers.get(0),
                                         note, super))) {
        this->errors->createNewError(DUPLICATE_CLASS, ast->line, ast->col, " '" + name + "'");

        printNote(super->getChildClass(name)->note, "class `" + name + "` previously defined here");
        return super->getChildClass(name);
    } else
        return super->getChildClass(name);
}

void RuntimeEngine::removeScope() {
    currentScope()->free();
    scopeMap.pop_back();
}

void RuntimeEngine::parseClassDecl(Ast *ast)
{
    Scope* scope = currentScope();
    Ast* block = ast->getLastSubAst();
    List<AccessModifier> modifiers;
    ClassObject* currentClass;
    int startPosition=1;

    if(parseAccessDecl(ast, modifiers, startPosition)){
        parseClassAccessModifiers(modifiers, ast);
    } else {
        modifiers.add(PROTECTED);
    }

    string className =  ast->getEntity(startPosition).getToken();

    if(scope->klass == NULL)
        currentClass = addGlobalClassObject(className, modifiers, ast);
    else
        currentClass = addChildClassObject(className, modifiers, ast, scope->klass);


    addScope(Scope(CLASS_SCOPE, currentClass));
    for(long i = 0; i < block->getSubAstCount(); i++) {
        ast = block->getSubAst(i);
        CHECK_ERRORS

        switch(ast->getType()) {
            case ast_class_decl:
                parseClassDecl(ast);
                break;
            case ast_var_decl:
                parseVarDecl(ast);
                break;
            case ast_method_decl: /* Will be parsed later */
                break;
            case ast_operator_decl: /* Will be parsed later */
                break;
            case ast_construct_decl: /* Will be parsed later */
                break;
            default:
                stringstream err;
                err << ": unknown ast type: " << ast->getType();
                errors->createNewError(INTERNAL_ERROR, ast->line, ast->col, err.str());
                break;
        }
    }
    removeScope();
}

void RuntimeEngine::parseVarDecl(Ast *ast)
{
    Scope* scope = currentScope();
    List<AccessModifier> modifiers;
    int startpos=0;


    if(parseAccessDecl(ast, modifiers, startpos)){
        parseVarAccessModifiers(modifiers, ast);
    } else {
        modifiers.add(PUBLIC);
    }

    string name =  ast->getEntity(startpos).getToken();
    RuntimeNote note = RuntimeNote(activeParser->sourcefile, activeParser->getErrors()->getLine(ast->line),
                                   ast->line, ast->col);

    if(!scope->klass->addField(Field(NULL, uniqueSerialId++, name, scope->klass, modifiers, note))) {
        this->errors->createNewError(PREVIOUSLY_DEFINED, ast->line, ast->col,
                               "field `" + name + "` is already defined in the scope");
        printNote(note, "field `" + name + "` previously defined here");
    }
}

int RuntimeEngine::parseVarAccessSpecifiers(List<AccessModifier> &modifiers) {
    for(long i = 0; i < modifiers.size(); i++) {
        AccessModifier modifier = modifiers.get(i);
        if(modifier > STATIC)
            return i;
    }

    if(modifiers.get(0) <= PROTECTED) {
        if(modifiers.size() > 3)
            return (int)(modifiers.size() - 1);
        else if(modifiers.size() == 2) {
            if((modifiers.get(1) != mCONST
                && modifiers.get(1) != STATIC))
                return 1;
        }
        else if(modifiers.size() == 3) {
            if(modifiers.get(1) != STATIC)
                return 1;
            if(modifiers.get(2) != mCONST)
                return 2;
        }
    }
    else if(modifiers.get(0) == STATIC) {
        if(modifiers.size() > 2)
            return (int)(modifiers.size() - 1);
        else if(modifiers.size() == 2 && modifiers.get(1) != mCONST)
            return 1;
    }
    else if(modifiers.get(0) == mCONST) {
        if(modifiers.size() != 1)
            return (int)(modifiers.size() - 1);
    }
    return -1;
}

void RuntimeEngine::parseVarAccessModifiers(List<AccessModifier> &modifiers, Ast *ast) {
    if(modifiers.size() > 3)
        this->errors->createNewError(GENERIC, ast->line, ast->col, "too many access specifiers");
    else {
        int result = parseVarAccessSpecifiers(modifiers);
        switch(result) {
            case -1:
                break;
            default:
                this->errors->createNewError(INVALID_ACCESS_SPECIFIER, ast->getEntity(result).getLine(),
                                             ast->getEntity(result).getColumn(), " `" + ast->getEntity(result).getToken() + "`");
                break;
        }
    }

    if(!modifiers.find(PUBLIC) && !modifiers.find(PRIVATE)
       && !modifiers.find(PROTECTED)) {
        modifiers.add(PUBLIC);
    }
}

void RuntimeEngine::generate() {

}

void RuntimeEngine::cleanup() {
    for(unsigned long i = 0; i < parsers.size(); i++) {
        parsers.get(i)->free();
    }
    parsers.free();
    if(errors != NULL)
    {
        errors->free();
        delete(errors); errors = NULL;
    }
    modules.free();

    for(unsigned long i = 0; i < importMap.size(); i++) {
        importMap.get(i).value.free();
        importMap.get(i).key.clear();
    }

    for(unsigned long i = 0; i < inline_map.size(); i++) {
        inline_map.get(i).key.clear();
    }

    importMap.free();
    freeList(classes);
    freeList(scopeMap);
    sourceFiles.free();
    lastNoteMsg.clear();
    exportFile.clear();
    noteMessages.free();
    inline_map.free();
    stringMap.free();
}
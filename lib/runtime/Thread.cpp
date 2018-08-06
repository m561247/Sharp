//
// Created by BraxtonN on 2/12/2018.
//

#include <cmath>
#include "Thread.h"
#include "memory/GarbageCollector.h"
#include "Exe.h"
#include "Environment.h"
#include "VirtualMachine.h"
#include "Opcode.h"
#include "register.h"
#include "Manifest.h"
#include "oo/Object.h"
#include "../util/time.h"

#ifdef WIN32_
#include <conio.h>
#elif defined(POSIX_)
#include "termios.h"
#endif

int32_t Thread::tid = 0;
thread_local Thread* thread_self = NULL;
List<Thread*> Thread::threads;

#ifdef WIN32_
std::mutex Thread::threadsMonitor;
#endif
#ifdef POSIX_
std::mutex Thread::threadsMonitor;
#endif
bool Thread::isAllThreadsSuspended = false;

/*
 * Local registers for the thread to use
 */
thread_local double registers[12];

void Thread::Startup() {
    threads.init();
    threads.setMax(MAX_THREADS);

    Thread* main = (Thread*)malloc(
            sizeof(Thread)*1);
    main->main = &env->methods[manifest.entryMethod];
    main->Create("Main");
#ifdef WIN32_
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    SetPriorityClass(GetCurrentThread(), HIGH_PRIORITY_CLASS);
#endif
    setupSigHandler();
}

/**
 * API level thread create called from Sharp
 *
 * @param methodAddress
 * @param stack_size
 * @return
 */
int32_t Thread::Create(int32_t methodAddress, unsigned long stack_size) {
    if(methodAddress < 0 || methodAddress >= manifest.methods)
        return -1;
    Method* method = &env->methods[methodAddress];
    if(method->paramSize>0)
        return -2;
    if(stack_size <= method->paramSize)
        return -3;

    Thread* thread = (Thread*)malloc(
            sizeof(Thread)*1);

#ifdef WIN32_
    new (&thread->mutex) std::mutex();
#endif
#ifdef POSIX_
    new (&thread->mutex) std::mutex();
#endif
    thread->name.init();
    thread->rand = new Random();
    thread->main = method;
    thread->id = Thread::tid++;
    thread->dataStack = NULL;
    thread->callStack = NULL;
    thread->currentThread.object=NULL;
    thread->args.object=NULL;
    thread->calls=0;
    thread->starting=0;
    thread->current = NULL;
    thread->priority=THREAD_PRIORITY_NORM;
    thread->suspendPending = false;
    thread->exceptionThrown = false;
    thread->suspended = false;
    thread->throwable.init();
    thread->exited = false;
    thread->daemon = false;
    thread->terminated = false;
    thread->state = THREAD_CREATED;
    thread->exitVal = 0;
    thread->stack_lmt = stack_size;
    thread->fp=0;
    thread->sp=NULL;

    pushThread(thread);

    stringstream ss;
    ss << "Thread@" << thread->id;
    thread->name = ss.str();
    return thread->id;
}

void Thread::Create(string name) {
#ifdef WIN32_
    new (&mutex) std::mutex();
#endif
#ifdef POSIX_
    new (&this->mutex) std::mutex();
#endif
    this->name.init();
    this->currentThread.object=NULL;
    this->args.object=NULL;
    this->name = name;
    this->starting = 0;
    this->rand = new Random();
    this->id = Thread::tid++;
    this->dataStack = (StackElement*)__malloc(sizeof(StackElement)*STACK_SIZE);
    this->suspendPending = false;
    this->exceptionThrown = false;
    this->suspended = false;
    this->exited = false;
    this->priority=THREAD_PRIORITY_HIGH;
    this->throwable.init();
    this->daemon = false;
    this->terminated = false;
    this->state = THREAD_CREATED;
    this->exitVal = 0;
    this->stack_lmt = STACK_SIZE;
    this->callStack = NULL;
    this->calls=0;
    this->fp=dataStack;
    this->sp=dataStack-1;

    for(unsigned long i = 0; i < STACK_SIZE; i++) {
        this->dataStack[i].object.object = NULL;
        this->dataStack[i].var=0;
    }

    pushThread(this);
}

void Thread::CreateDaemon(string name) {
#ifdef WIN32_
    new (&mutex) std::mutex();
#endif
#ifdef POSIX_
    new (&this->mutex) std::mutex();
#endif
    this->name.init();
    this->name = name;
    this->id = Thread::tid++;
    this->rand = new Random();
    this->dataStack = NULL;
    this->callStack = NULL;
    this->currentThread.object=NULL;
    this->args.object=NULL;
    this->calls=0;
    this->starting=0;
    this->priority=THREAD_PRIORITY_NORM;
    this->current = NULL;
    this->suspendPending = false;
    this->exceptionThrown = false;
    this->suspended = false;
    this->exited = false;
    this->daemon = true;
    this->terminated = false;
    this->throwable.init();
    this->state = THREAD_CREATED;
    this->exitVal = 0;
    this->stack_lmt=0;
    this->fp=0;
    this->sp=NULL;
    this->main=NULL;

    pushThread(this);
}

void Thread::pushThread(Thread *thread) {
    std::lock_guard<std::mutex> guard(threadsMonitor);
    threads.push_back(thread);
}

void Thread::popThread(Thread *thread) {
    std::lock_guard<std::mutex> guard(threadsMonitor);
    threads.removefirst(thread);
}

Thread *Thread::getThread(int32_t id) {
    for(int32_t i = 0 ; i < threads.size(); i++) {
        if(threads.get(i) != NULL && threads.get(i)->id == id)
            return threads.get(i);
    }

    return NULL;
}

void Thread::suspendSelf() {
    thread_self->suspended = true;
    thread_self->suspendPending = false;

    /*
	 * We call wait upon suspend. This function will
	 * sleep the thread most of the time. unsuspendThread() or
	 * resumeAllThreads() should be called to revive thread.
	 */
    thread_self->wait();
}

void Thread::wait() {
    const long sMaxRetries = 128 * 1024;

    long spinCount = 0;
    long retryCount = 0;

    this->state = THREAD_SUSPENDED;

    while (this->suspended)
    {
        if (retryCount++ == sMaxRetries)
        {
            spinCount++;
            retryCount = 0;
#ifdef WIN32_
            Sleep(5);
#endif
#ifdef POSIX_
            usleep(5*POSIX_USEC_INTERVAL);
#endif
        } else if(this->state == THREAD_KILLED) {
            this->suspended = false;
            return;
        }
    }

    this->state = THREAD_RUNNING;
}

int Thread::start(int32_t id) {
    Thread *thread = getThread(id);

    if (thread == NULL || thread->starting || id==main_threadid)
        return 1;

    if(thread->state == THREAD_RUNNING)
        return 2;

    if(thread->terminated)
        return 4;

    thread->exited = false;
    thread->state = THREAD_CREATED;
    thread->starting = 1;
#ifdef WIN32_
    thread->thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            &vm->InterpreterThreadStart,       // thread function caller
            thread,                 // thread self when thread is created
            0,                      // use default creation flags
            NULL);
    if(thread->thread == NULL) return 3; // thread was not started
    else return waitForThread(thread);
#endif
#ifdef POSIX_
    if(pthread_create( &thread->thread, NULL, vm->InterpreterThreadStart, (void*) thread))
        return 3; // thread was not started
    else {
        return waitForThread(thread);
    }
#endif

}

int Thread::destroy(int64_t id) {
    if(id == thread_self->id || id==main_threadid)
        return 1; // cannot destroy thread_self or main

    Thread* thread = getThread(id);
    if(thread == NULL || thread->daemon)
        return 1;

    if (thread->state == THREAD_KILLED || thread->terminated)
    {
        if (thread->id == main_threadid)
        {
            return 3; // should never happen
        }
        else
        {
            popThread(thread);
            thread->term();
            std::free (thread);
            return 0;
        }
    } else {
        return 2;
    }
}

int Thread::interrupt(int32_t id) {
    if(id == thread_self->id)
        return 1; // cannot interrupt thread_self

    Thread* thread = getThread(id);
    if(thread == NULL || thread->daemon)
        return 1;
    if(thread->terminated)
        return 2;

    int result = interrupt(thread);
    if(result==0 && !masterShutdown)
        waitForThreadExit(thread);
    return result;
}

void Thread::waitForThreadSuspend(Thread *thread) {
    const int sMaxRetries = 10000000;
    const int sMaxSpinCount = 25;

    int spinCount = 0;
    int retryCount = 0;

    suspendThread(thread);
    while (thread->state == THREAD_RUNNING && !thread->suspended)
    {
        if (retryCount++ == sMaxRetries)
        {
            retryCount = 0;
            if(++spinCount >= sMaxSpinCount)
            {
                return; // give up
            } else if(thread->state >= THREAD_SUSPENDED)
                return;
        }
    }
}

void Thread::waitForThreadExit(Thread *thread) {
    const int sMaxRetries = 10000000;
    int retryCount = 0;

    while (!thread->exited)
    {
        if (retryCount++ == sMaxRetries)
        {
            retryCount = 0;
            if(thread->exited)
                return;

            __os_sleep(1);
        }
    }
}

void Thread::terminateAndWaitForThreadExit(Thread *thread) {
    const int sMaxRetries = 10000000;
    int retryCount = 0;

    thread->term();
    while (!thread->exited)
    {
        if (retryCount++ == sMaxRetries)
        {
            retryCount = 0;
            if(thread->exited)
                return;

            __os_sleep(1);
        }

        thread->state = THREAD_KILLED;
    }
}

int Thread::waitForThread(Thread *thread) {
    const int sMaxRetries = 100000000;
    const int sMaxSpinCount = 25;

    int spinCount = 0;
    int retryCount = 0;

    while (thread->state == THREAD_CREATED
           || thread->state == THREAD_SUSPENDED)
    {
        if (retryCount++ == sMaxRetries)
        {
            retryCount = 0;
            if(++spinCount >= sMaxSpinCount)
            {
                return -255; // give up
            }
            __os_sleep(1);
        }
    }
    return 0;
}

void Thread::suspendAllThreads() {
    Thread* thread;
    isAllThreadsSuspended = true;

    for(unsigned int i= 0; i < threads.size(); i++) {
        thread=threads.get(i);

        if(thread!=NULL &&
           (thread->id != thread_self->id) && thread->state == THREAD_RUNNING){
            suspendThread(thread);
            waitForThreadSuspend(thread);
        }
    }
}

void Thread::resumeAllThreads() {
    Thread* thread;
    isAllThreadsSuspended = false;

    for(unsigned int i= 0; i < threads.size(); i++) {
        thread=threads.get(i);

        if(thread!=NULL &&
           (thread->id != thread_self->id)){
            unsuspendThread(thread);
        }
    }
}

int Thread::unsuspendThread(Thread *thread) {
    thread->suspended = false;
    return 0;
}

void Thread::suspendThread(Thread *thread) {
    if(thread->id == thread_self->id)
        suspendSelf();
    else {
        std::lock_guard<std::mutex> gd(thread->mutex);
        thread->suspendPending = true;
    }
}

void Thread::term() {
    this->state = THREAD_KILLED;
    this->terminated = true;
    if(dataStack != NULL) {
        for(unsigned long i = 0; i < this->stack_lmt; i++) {
            GarbageCollector::self->releaseObject(&dataStack[i].object);
        }
        std::free(dataStack); dataStack = NULL;
    }

    if(callStack != NULL) {
        std::free(callStack); callStack = NULL;
    }

    if(rand != NULL) {
        delete rand;
    }

#ifdef SHARP_PROF_
    tprof.free();
#endif
    this->name.free();
}

int Thread::join(int32_t id) {
    if (id == thread_self->id || id==main_threadid)
        return 1;

    Thread* thread = getThread(id);
    if (thread == NULL || thread->daemon)
        return 1;
    if(thread->terminated)
        return 2;

    return threadjoin(thread);
}

int Thread::threadjoin(Thread *thread) {
    if(thread->starting)
        waitForThread(thread);

    if (thread->state == THREAD_RUNNING)
    {
#ifdef WIN32_
        WaitForSingleObject(thread->thread, INFINITE);
        return 0;
#endif
#ifdef POSIX_
        if(pthread_join(thread->thread, NULL))
            return 3;
        else return 0;
#endif
    }

    return 2;
}

void Thread::killAll() {
    Thread* thread;
    suspendAllThreads();

    for(unsigned int i = 0; i < threads.size(); i++) {
        thread = threads.get(i);

        if(thread != NULL && thread->id != thread_self->id
           && thread->state != THREAD_KILLED && thread->state != THREAD_CREATED) {
            if(thread->state == THREAD_RUNNING){
                interrupt(thread);
            }

            terminateAndWaitForThreadExit(thread);
        } else if(thread != NULL){

            thread->term();
        }
    }
}

int Thread::interrupt(Thread *thread) {
    if (thread->state == THREAD_RUNNING)
    {
        if (thread->id == main_threadid)
        {
            /*
            * Shutdown all running threads
            * and de-allocate all allocated
            * memory. If we do not call join()
            * to wait for all other threads
            * regardless of what they are doing, we
            * stop them.
            */
            vm->shutdown();
            masterShutdown = true;
        }
        else
        {
            std::lock_guard<std::mutex> gd(thread->mutex);
            thread->state = THREAD_KILLED; // terminate thread
            return 0;
        }
    }

    return 2;
}

void Thread::shutdown() {
    if(!threads.empty()) {
        Thread::killAll();

        for(unsigned int i = 0; i < threads.size(); i++) {
            if(threads.get(i) != NULL) {
                std::free(threads.get(i));
            }
        }

        threads.free();
    }
}

void Thread::exit() {

    if(this->exceptionThrown) {
        this->exitVal = -800;
        cout << throwable.stackTrace.str();
        cout << endl << throwable.throwable->name.str() << " "
             << throwable.message.str() << "\n";
    } else {
        if(!daemon && dataStack)
            this->exitVal = (int)dataStack[0].var;
        else
            this->exitVal = 0;
    }

    if(dataStack != NULL) {
        StackElement *p = dataStack;
        for(size_t i = 0; i < stack_lmt; i++)
        {
            if(p->object.object) {
                DEC_REF(p->object.object);
                p->object.object=NULL;
            }
            p++;
        }
    }

    free(this->callStack); callStack = NULL;
    this->state = THREAD_KILLED;
    this->exited = true;
}

int Thread::startDaemon(
#ifdef WIN32_
        DWORD WINAPI
#endif
#ifdef POSIX_
void*
#endif
(*threadFunc)(void *), Thread *thread) {
    if (thread == NULL || !thread->daemon)
        return 1;

    if(thread->state == THREAD_RUNNING)
        return 2;

    thread->exited = false;
#ifdef WIN32_
    thread->thread = CreateThread(
            NULL,                   // default security attributes
            0,                      // use default stack size
            (LPTHREAD_START_ROUTINE)threadFunc,       // thread function caller
            thread,                 // thread self when thread is created
            0,                      // use default creation flags
            NULL);
    if(thread->thread == NULL) return 3; // thread was not started
    else
        return waitForThread(thread);
#endif
#ifdef POSIX_
    if(pthread_create( &thread->thread, NULL, threadFunc, (void*) thread)!=0)
        return 3; // thread was not started
    else
        return waitForThread(thread);
#endif
}

#ifdef DEBUGGING
void printRegs() {
    cout << endl;
    cout << "Registers: \n";
    cout << "adx = " << registers[adx] << endl;
    cout << "cx = " << registers[cx] << endl;
    cout << "cmt = " << registers[cmt] << endl;
    cout << "ebx = " << registers[ebx] << endl;
    cout << "ecx = " << registers[ecx] << endl;
    cout << "ecf = " << registers[ecf] << endl;
    cout << "edf = " << registers[edf] << endl;
    cout << "ehf = " << registers[ehf] << endl;
    cout << "bmr = " << registers[bmr] << endl;
    cout << "egx = " << registers[egx] << endl;
    cout << "sp -> " << (thread_self->sp-thread_self->dataStack) << endl;
    cout << "fp -> " << (thread_self->fp-thread_self->dataStack) << endl;
    cout << "pc -> " << PC(thread_self) << endl;
    if(thread_self->current != NULL) {
        cout << "current -> " << thread_self->current->name.str() << endl;
    }
    native_string stackTrace;

    vm->fillStackTrace(stackTrace);
    cout << "stacktrace ->\n\n " << stackTrace.str() << endl;
    cout << endl;
}
#endif

double exponent(int64_t n){
    if (n < 100000){
        // 5 or less
        if (n < 100){
            // 1 or 2
            if (n < 10)
                return n*0.1;
            else
                return n*0.01;
        }else{
            // 3 or 4 or 5
            if (n < 1000)
                return n*0.001;
            else{
                // 4 or 5
                if (n < 10000)
                    return n*0.0001;
                else
                    return n*0.00001;
            }
        }
    } else {
        // 6 or more
        if (n < 10000000) {
            // 6 or 7
            if (n < 1000000)
                return n*0.000001;
            else
                return n*0.0000001;
        } else if(n < 1000000000) {
            // 8 to 10
            if (n < 100000000)
                return n*0.00000001;
            else {
                // 9 or 10
                if (n < 1000000000)
                    return n*0.000000001;
                else
                    return n*0.0000000001;
            }
        } else if(n < 1000000000000000) {
            // 11 to 15
            if (n < 100000000000)
                return n*0.00000000001;
            else {
                // 12 to 15
                if (n < 1000000000000)
                    return n*0.000000000001;
                else if (n < 10000000000000)
                    return n*0.0000000000001;
                else if (n < 100000000000000)
                    return n*0.00000000000001;
                else
                    return n*0.000000000000001;
            }
        }
        else {
            return n*0.0000000000000001;
        }
    }
}

/**
 * This tells the virtual machine to process code
 * when executing a finally block or the regular processing
 * of a function.
 */
short int startAddress = 0;

/*
 * We need this to keep track of which finally block we are executing
 */
FinallyTable finallyTable;
unsigned long long irCount = 0, overflow = 0;

void Thread::exec() {

    register int64_t tmp=0;
    register int64_t val=0;
    register int64_t delegate=0;
    register int64_t args=0;
    ClassObject *klass;
    SharpObject* o=NULL;
    Method* f;
    Object* o2=NULL;
    void* opcodeStart = (startAddress == 0) ?  (&&interp) : (&&finally) ;
    Method* finnallyMethod;

#ifdef SHARP_PROF_
    tprof.init(stack_lmt);
    tprof.starttm=Clock::realTimeInNSecs();
    for(size_t i = 0; i < manifest.methods; i++) {
        tprof.functions.push_back();
        tprof.functions.last().init();
        tprof.functions.last() = funcProf(env->methods+i);
    }
#endif

#ifdef SHARP_PROF_
    tprof.hit(main);
#endif

    _initOpcodeTable
    try {
        for (;;) {
            /* We dont want to always run finally code when we start a thread */
            if(startAddress == 0) goto interp;
            finnallyMethod = current;

            finally:
            if((PC(this) <finallyTable.start_pc || PC(this) > finallyTable.end_pc)
               && current==finnallyMethod)
                return;

            interp:
            if(current->address==2 && PC(this) >= 2) {
                int i = 0;
            }
            DISPATCH();
            _NOP:
                _brh
            _INT:

#ifdef SHARP_PROF_
            if(GET_Da(*pc) == 0xa9) {
                tprof.endtm=Clock::realTimeInNSecs();
                tprof.profile();
                tprof.dump();
            }

#endif
                vm->sysInterrupt(GET_Da(*pc));
                if(masterShutdown) return;
                _brh
            _MOVI:
                registers[*(pc+1)]=GET_Da(*pc);
                _brh_inc(2)
            RET:
                if(calls <= 1) {
#ifdef SHARP_PROF_
                tprof.endtm=Clock::realTimeInNSecs();
                tprof.profile();
#endif
                    return;
                }

                Frame *frame = callStack+(calls--);

                if(current->finallyBlocks.len > 0)
                    vm->executeFinally(thread_self->current);

                current = frame->last;
                cache = current->bytecode;

                pc = frame->pc;
                sp = frame->sp;
                fp = frame->fp;

#ifdef SHARP_PROF_
            tprof.profile();
#endif
                _brh
            HLT:
                state=THREAD_KILLED;
                _brh
            NEWARRAY:
                (++sp)->object =
                        GarbageCollector::self->newObject(registers[GET_Da(*pc)]);
                STACK_CHECK _brh
            CAST:
                CHECK_NULL(o2->castObject(registers[GET_Da(*pc)]);)
                _brh
            VARCAST:
                CHECK_NULL2(
                        if(o2->object->HEAD == NULL) {
                            castErr:
                            stringstream ss;
                            ss << "illegal cast to var" << (GET_Da(*pc) ? "[]" : "");
                            throw Exception(Environment::ClassCastException, ss.str());
                        }
                )
                _brh
            MOV8:
                registers[GET_Ca(*pc)]=(int8_t)registers[GET_Cb(*pc)];
                _brh
            MOV16:
                registers[GET_Ca(*pc)]=(int16_t)registers[GET_Cb(*pc)];
                _brh
            MOV32:
                registers[GET_Ca(*pc)]=(int32_t)registers[GET_Cb(*pc)];
                _brh
            MOV64:
                registers[GET_Ca(*pc)]=(int64_t)registers[GET_Cb(*pc)];
                _brh
            MOVU8:
                registers[GET_Ca(*pc)]=(uint8_t)registers[GET_Cb(*pc)];
                _brh
            MOVU16:
                registers[GET_Ca(*pc)]=(uint16_t)registers[GET_Cb(*pc)];
                _brh
            MOVU32:
                registers[GET_Ca(*pc)]=(uint32_t)registers[GET_Cb(*pc)];
                _brh
            MOVU64:
                registers[GET_Ca(*pc)]=(uint64_t)registers[GET_Cb(*pc)];
                _brh
            RSTORE:
                (++sp)->var = registers[GET_Da(*pc)];
                STACK_CHECK _brh
            ADD:
                registers[*(pc+1)]=registers[GET_Ca(*pc)]+registers[GET_Cb(*pc)];
                _brh_inc(2)
            SUB:
                registers[*(pc+1)]=registers[GET_Ca(*pc)]-registers[GET_Cb(*pc)];
                _brh_inc(2)
            MUL:
                registers[*(pc+1)]=registers[GET_Ca(*pc)]*registers[GET_Cb(*pc)];
                _brh_inc(2)
            DIV:
                if(registers[GET_Ca(*pc)]==0 && registers[GET_Cb(*pc)]==0) throw Exception("divide by 0");
                registers[*(pc+1)]=registers[GET_Ca(*pc)]/registers[GET_Cb(*pc)];
                _brh_inc(2)
            MOD:
                registers[*(pc+1)]=(int64_t)registers[GET_Ca(*pc)]%(int64_t)registers[GET_Cb(*pc)];
                _brh_inc(2)
            IADD:
                registers[GET_Ca(*pc)]+=GET_Cb(*pc);
                _brh
            ISUB:
                registers[GET_Ca(*pc)]-=GET_Cb(*pc);
                _brh
            IMUL:
                registers[GET_Ca(*pc)]*=GET_Cb(*pc);
                _brh
            IDIV:
                if(GET_Cb(*pc)==0) throw Exception("divide by 0");
                registers[GET_Ca(*pc)]/=GET_Cb(*pc);
                _brh
            IMOD:
                registers[GET_Ca(*pc)]=(int64_t)registers[GET_Ca(*pc)]%(int64_t)GET_Cb(*pc);
                _brh
            POP:
                --sp;
                _brh
            INC:
                registers[GET_Da(*pc)]++;
                _brh
            DEC:
                registers[GET_Da(*pc)]--;
                _brh
            MOVR:
                registers[GET_Ca(*pc)]=registers[GET_Cb(*pc)];
                _brh
            IALOAD:
                o = sp->object.object;
                if(o != NULL && o->HEAD != NULL) {
                    registers[GET_Ca(*pc)] = o->HEAD[(uint64_t)registers[GET_Cb(*pc)]];
                } else throw Exception(Environment::NullptrException, "");
                _brh
            BRH:
                pc=cache+(int64_t)registers[adx];
                _brh_NOINCREMENT
            IFE:
                if(registers[cmt]) {
                    pc=cache+(int64_t)registers[adx]; _brh_NOINCREMENT
                } else  _brh
            IFNE:
                if(registers[cmt]==0) {
                    pc=cache+(int64_t)registers[adx]; _brh_NOINCREMENT
                } else  _brh
            LT:
                registers[cmt]=registers[GET_Ca(*pc)]<registers[GET_Cb(*pc)];
                _brh
            GT:
                registers[cmt]=registers[GET_Ca(*pc)]>registers[GET_Cb(*pc)];
                _brh
            LTE:
                registers[cmt]=registers[GET_Ca(*pc)]<=registers[GET_Cb(*pc)];
                _brh
            GTE:
                registers[cmt]=registers[GET_Ca(*pc)]>=registers[GET_Cb(*pc)];
                _brh
            MOVL:
                o2 = &(fp+GET_Da(*pc))->object;
                _brh
            POPL:
            (fp+GET_Da(*pc))->object
                        = (sp--)->object.object;
                _brh
            IPOPL:
                (fp+GET_Da(*pc))->var
                        = (sp--)->var;
                _brh
            MOVSL:
                o2 = &((sp+GET_Da(*pc))->object);
                _brh
            MOVBI:
                registers[bmr]=GET_Da(*pc) + exponent(*(pc+1)); pc++;
                _brh
            SIZEOF:
                if(o2==NULL || o2->object == NULL)
                    registers[GET_Da(*pc)] = 0;
                else
                    registers[GET_Da(*pc)]=o2->object->size;
                _brh
            PUT:
                cout << registers[GET_Da(*pc)];
                _brh
            PUTC:
                printf("%c", (char)registers[GET_Da(*pc)]);
                _brh
            GET:
                if(CMT)
                    registers[GET_Da(*pc)] = getche();
                else
                    registers[GET_Da(*pc)] = getch();
                _brh
            CHECKLEN:
                CHECK_NULL2(
                        if((registers[GET_Da(*pc)]<o2->object->size) &&!(registers[GET_Da(*pc)]<0)) { _brh }
                        else {
                            stringstream ss;
                            ss << "Access to Object at: " << registers[GET_Da(*pc)] << " size is " << o2->object->size;
                            throw Exception(Environment::IndexOutOfBoundsException, ss.str());
                        }
                )
            GOTO:
                pc = cache+GET_Da(*pc);
                _brh_NOINCREMENT
            LOADPC:
                registers[GET_Da(*pc)] = PC(this);
                _brh
            PUSHOBJ:
                (++sp)->object = o2;
                STACK_CHECK _brh
            DEL:
            GarbageCollector::self->releaseObject(o2);
                _brh
            CALL:
#ifdef SHARP_PROF_
            tprof.hit(env->methods+GET_Da(*pc));
#endif
                CALLSTACK_CHECK
                executeMethod(GET_Da(*pc), this)
                _brh_NOINCREMENT
            CALLD:
#ifdef SHARP_PROF_
            tprof.hit(env->methods+GET_Da(*pc));
#endif
                if((val = (int64_t )registers[GET_Da(*pc)]) <= 0 || val >= manifest.methods) {
                    stringstream ss;
                    ss << "invalid call to pointer of " << val;
                    throw Exception(ss.str());
                }
                CALLSTACK_CHECK
                executeMethod(val, this)
                _brh_NOINCREMENT
            NEWCLASS:
                (++sp)->object =
                        GarbageCollector::self->newObject(&env->classes[GET_Da(*pc)]);
                STACK_CHECK _brh
            MOVN:
                CHECK_NULLOBJ(
                        if(GET_Da(*pc) >= o2->object->size)
                            throw Exception("movn");

                        o2 = &o2->object->node[GET_Da(*pc)];
                )
                _brh
            SLEEP:
                __os_sleep((int64_t)registers[GET_Da(*pc)]);
                _brh
            TEST:
                registers[cmt]=registers[GET_Ca(*pc)]==registers[GET_Cb(*pc)];
                _brh
            TNE:
                registers[cmt]=registers[GET_Ca(*pc)]!=registers[GET_Cb(*pc)];
                _brh
            LOCK:
                CHECK_NULLOBJ(o2->monitorLock();)
                _brh
            ULOCK:
                CHECK_NULLOBJ(o2->monitorUnLock();)
                _brh
            EXP:
                registers[bmr] = exponent(registers[GET_Da(*pc)]);
                _brh
            MOVG:
                o2 = env->globalHeap+GET_Da(*pc);
                _brh
            MOVND:
                if(o2 != NULL && o2->object != NULL) {
                    if(o2->object->size <= registers[GET_Da(*pc)]) {
                        throw Exception("movnd");
                    }
                }
                CHECK_NULLOBJ(o2 = &o2->object->node[(int64_t)registers[GET_Da(*pc)]];)
                _brh
            NEWOBJARRAY:
                (++sp)->object = GarbageCollector::self->newObjectArray(registers[GET_Da(*pc)]);
                STACK_CHECK _brh
            NOT:
                registers[GET_Ca(*pc)]=!registers[GET_Cb(*pc)];
                _brh
            SKIP:
                pc = pc+GET_Da(*pc);
                _brh
            LOADVAL:
                registers[GET_Da(*pc)]=(sp--)->var;
                _brh
            SHL:
                registers[*(pc+1)]=(int64_t)registers[GET_Ca(*pc)]<<(int64_t)registers[GET_Cb(*pc)];
                _brh_inc(2)
            SHR:
                registers[*(pc+1)]=(int64_t)registers[GET_Ca(*pc)]>>(int64_t)registers[GET_Cb(*pc)];
                _brh_inc(2)
            SKPE:
                if(registers[cmt]) {
                    pc = pc+GET_Da(*pc); _brh_NOINCREMENT
                } else _brh
            SKNE:
                if(registers[cmt]==0) {
                    pc = pc+GET_Da(*pc); _brh_NOINCREMENT
                } else _brh
            CMP:
                registers[cmt]=registers[GET_Ca(*pc)]==GET_Cb(*pc);
                _brh
            AND:
                registers[cmt]=registers[GET_Ca(*pc)]&&registers[GET_Cb(*pc)];
                _brh
            UAND:
                registers[cmt]=(int64_t)registers[GET_Ca(*pc)]&(int64_t)registers[GET_Cb(*pc)];
                _brh
            OR:
                registers[cmt]=(int64_t)registers[GET_Ca(*pc)]|(int64_t)registers[GET_Cb(*pc)];
                _brh
            UNOT:
                registers[cmt]=(int64_t)registers[GET_Ca(*pc)]^(int64_t)registers[GET_Cb(*pc)];
                _brh
            THROW:
                throw Exception("", false);
                _brh
            CHECKNULL:
                CHECK_NULL(registers[cmt]=o2->object==NULL;)
                _brh
            RETURNOBJ:
                fp->object=o2;
                _brh
            NEWCLASSARRAY:
                CHECK_NULL(
                        (++sp)->object = GarbageCollector::self->newObjectArray(registers[GET_Ca(*pc)],
                                                                                        env->findClassBySerial(GET_Cb(*pc)));
                )
                STACK_CHECK _brh
            NEWSTRING:
                GarbageCollector::self->createStringArray(&(++sp)->object, env->getStringById(GET_Da(*pc)));
                STACK_CHECK _brh
            ADDL:
                (fp+GET_Cb(*pc))->var+=registers[GET_Ca(*pc)];
                _brh
            SUBL:
                (fp+GET_Cb(*pc))->var-=registers[GET_Ca(*pc)];
                _brh
            MULL:
                (fp+GET_Cb(*pc))->var*=registers[GET_Ca(*pc)];
                _brh
            DIVL:
                (fp+GET_Cb(*pc))->var/=registers[GET_Ca(*pc)];
                _brh
            MODL:
                (fp+GET_Cb(*pc))->modul(registers[GET_Ca(*pc)]);
                _brh
            IADDL:
                (fp+GET_Cb(*pc))->var+=GET_Ca(*pc);
                _brh
            ISUBL:
                (fp+GET_Cb(*pc))->var-=GET_Ca(*pc);
                _brh
            IMULL:
                (fp+GET_Cb(*pc))->var*=GET_Ca(*pc);
                _brh
            IDIVL:
                (fp+GET_Cb(*pc))->var/=GET_Ca(*pc);
                _brh
            IMODL:
                val = (fp+GET_Cb(*pc))->var;
                (fp+GET_Cb(*pc))->var=val%GET_Ca(*pc);
                _brh
            LOADL:
                registers[GET_Ca(*pc)]=(fp+GET_Cb(*pc))->var;
                _brh
            IALOAD_2:
                CHECK_INULLOBJ(
                        registers[GET_Ca(*pc)] = o2->object->HEAD[(uint64_t)registers[GET_Cb(*pc)]];
                )
                _brh
            POPOBJ:
                CHECK_NULL(
                        *o2 = (sp--)->object;
                )
                _brh
            SMOVR:
                (sp+GET_Cb(*pc))->var=registers[GET_Ca(*pc)];
                _brh
            SMOVR_2:
                (fp+GET_Cb(*pc))->var=registers[GET_Ca(*pc)];
                _brh
            ANDL:
                (fp+GET_Cb(*pc))->andl(registers[GET_Ca(*pc)]);
                _brh
            ORL:
                (fp+GET_Cb(*pc))->orl(registers[GET_Ca(*pc)]);
                _brh
            NOTL:
                (fp+GET_Cb(*pc))->notl(registers[GET_Ca(*pc)]);
                _brh
            RMOV:
                CHECK_INULLOBJ(
                        o2->object->HEAD[(int64_t)registers[GET_Ca(*pc)]]=registers[GET_Cb(*pc)];
                )
                _brh
            SMOV:
                registers[GET_Ca(*pc)]=(sp+GET_Cb(*pc))->var;
                _brh
            LOADPC_2:
                registers[GET_Ca(*pc)]=PC(this)+GET_Cb(*pc);
                _brh
            RETURNVAL:
                (fp)->var=registers[GET_Da(*pc)];
                _brh
            ISTORE:
                (++sp)->var = GET_Da(*pc);
                STACK_CHECK _brh
            ISTOREL:
                (fp+GET_Da(*pc))->var=*(pc+1);
                _brh_inc(2)
            PUSHNIL:
            GarbageCollector::self->releaseObject(&(++sp)->object);
                STACK_CHECK _brh
            IPUSHL:
                (++sp)->var = (fp+GET_Da(*pc))->var;
                STACK_CHECK _brh
            PUSHL:
                (++sp)->object = (fp+GET_Da(*pc))->object;
                STACK_CHECK _brh
            ITEST:
                o2 = &(sp--)->object;
                registers[GET_Da(*pc)] = o2->object == (sp--)->object.object;
                _brh
            INVOKE_DELEGATE:
                delegate= GET_Ca(*pc);
                args= GET_Cb(*pc);

                o2 = &(sp-args)->object;


                CHECK_NULL2(
                        klass = o2->object->k;
                        if(klass!= NULL) {
                            search:
                            for(long i = 0; i < klass->methodCount; i++) {
                                if(env->methods[klass->methods[i]].delegateAddress == delegate) {
                                    CALLSTACK_CHECK
                                    executeMethod(env->methods[klass->methods[i]].address, this)
                                    _brh_NOINCREMENT
                                }
                            }

                            if(klass->base != NULL) {
                                klass = klass->base;
                                goto search;
                            }
                            throw Exception(Environment::RuntimeErr, "delegate function has no subscribers");
                        } else {
                            throw Exception(Environment::RuntimeErr, "attempt to call delegate function on non class object");
                        }
                )
                _brh
            INVOKE_DELEGATE_STATIC:
                delegate= GET_Ca(*pc);
                args= GET_Cb(*pc);

                o2 = &env->globalHeap[*(++pc)];

                CHECK_NULL2(
                        klass = o2->object->k;
                        if(klass!= NULL) {
                            for(long i = 0; i < klass->methodCount; i++) {
                                if(env->methods[klass->methods[i]].delegateAddress == delegate) {
                                    CALLSTACK_CHECK
                                    executeMethod(env->methods[klass->methods[i]].address, this)
                                    _brh_NOINCREMENT
                                }
                            }

                            if(klass->base != NULL) {
                                klass = klass->base;
                                goto search;
                            }
                            throw Exception(Environment::RuntimeErr, "delegate function has no subscribers");
                        } else {
                            throw Exception(Environment::RuntimeErr, "attempt to call delegate function on non class object");
                        }
                )
                _brh
            ISADD:
                (sp+GET_Cb(*pc))->var+=GET_Ca(*pc);
                _brh
            JE:
                if(registers[cmt]) {
                    pc=cache+GET_Da(*pc); _brh_NOINCREMENT
                } else  _brh
            JNE:
                if(registers[cmt]==0) {
                    pc=cache+GET_Da(*pc); _brh_NOINCREMENT
                } else  _brh
            SWITCH: {
                if((val = current->switchTable.get(GET_Da(*pc)).values.indexof(registers[ebx])) != -1 ) {
                    pc=cache+current->switchTable.get(GET_Da(*pc)).addresses.get(val);
                    _brh_NOINCREMENT
                } else {
                    pc=cache+current->switchTable.get(GET_Da(*pc)).defaultAddress;
                    _brh_NOINCREMENT
                }
            }


        }
    } catch (bad_alloc &e) {
        cout << "std::bad_alloc\n";
        // TODO: throw out of memory error
    } catch (Exception &e) {
        throwable = e.getThrowable();
        exceptionThrown = true;

        if(state == THREAD_KILLED) {
            vm->fillStackTrace(throwable.stackTrace);
            return;
        }
        vm->Throw(&sp->object);

        DISPATCH();
    }
}

void Thread::interrupt() {
    std::lock_guard<std::mutex> gd(mutex);

    if (suspendPending)
        suspendSelf();
    if (state == THREAD_KILLED)
        return;
}

void Thread::setup() {
    current = NULL;
    if(dataStack==NULL) {
        dataStack = (StackElement*)__malloc(sizeof(StackElement)*stack_lmt);
        GarbageCollector::self->addMemory(sizeof(StackElement)*stack_lmt);
    }
    if(callStack==NULL) {
        callStack = (Frame*)__malloc(sizeof(Frame)*stack_lmt);
        GarbageCollector::self->addMemory(sizeof(Frame)*stack_lmt);
    }
    calls=0;
    suspendPending = false;
    exceptionThrown = false;
    suspended = false;
    exited = false;
    terminated = false;
    exitVal = 0;
    starting = 0;

    if(id != main_threadid){
        int priority = (int)env->__sgetFieldVar("priority", currentThread.object);
        setPriority(this, priority);

        if(currentThread.object != nullptr
           && currentThread.object->k != nullptr) {
            Object *threadName = env->findField("name", currentThread.object);

            if(threadName != NULL) { // reset thread name
                env->createString(threadName, name);
            }
        }
        fp=dataStack;
        sp=dataStack-1;

        for(unsigned long i = 0; i < stack_lmt; i++) {
            this->dataStack[i].object.object = NULL;
            this->dataStack[i].var=0;
        }
    } else
        GarbageCollector::self->addMemory(sizeof(StackElement)*stack_lmt);
}

int Thread::setPriority(Thread* thread, int priority) {
    if(thread->thread != 0) {
        if(thread->priority == priority)
            return 0;

#ifdef WIN32_

        switch(priority) {
            case THREAD_PRIORITY_HIGH:
                SetThreadPriority(thread->thread, THREAD_PRIORITY_HIGHEST);
                break;
            case THREAD_PRIORITY_NORM:
                SetThreadPriority(thread->thread, THREAD_PRIORITY_ABOVE_NORMAL);
                break;
            case THREAD_PRIORITY_LOW:
                SetThreadPriority(thread->thread, THREAD_PRIORITY_BELOW_NORMAL);
                break;
        }
#endif
#ifdef POSIX_
        pthread_attr_t thAttr;
        int policy = 0;
        int pthread_prio = 0;

        pthread_attr_init(&thAttr);
        pthread_attr_getschedpolicy(&thAttr, &policy);

        if(priority == THREAD_PRIORITY_HIGH) {
            pthread_prio = sched_get_priority_max(policy);
        } else if(priority == THREAD_PRIORITY_LOW) {
            pthread_prio = sched_get_priority_min(policy);
        }

        pthread_setschedprio(thread->thread, pthread_prio);
        pthread_attr_destroy(&thAttr);
#endif
    }

    return 3;
}

int Thread::setPriority(int32_t id, int priority) {

    Thread* thread = getThread(id);
    if(thread == NULL || thread->daemon)
        return 1;
    if(thread->terminated || thread->state==THREAD_KILLED
       || thread->state==THREAD_CREATED)
        return 2;

    return setPriority(thread, priority);
}

void __os_sleep(int64_t INTERVAL) {
#ifdef WIN32_
    Sleep(INTERVAL);
#endif
#ifdef POSIX_
    usleep(INTERVAL);
#endif
}

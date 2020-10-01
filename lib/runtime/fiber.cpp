//
// Created by BNunnally on 9/5/2020.
//
#include "fiber.h"
#include "Thread.h"
#include "VirtualMachine.h"

uInt fiber::fibId=0;
atomic<uInt> listSize = {0}, fiberCount ={0};

const Int resizeCount = 500;
recursive_mutex fiberMutex;
atomic<fiber**> fibers = {NULL };

#define fiberAt(pos) fibers.load(std::memory_order_relaxed)[pos]

void increase_fibers() {
    GUARD(fiberMutex)
    fiber** tmpfibers = (fiber**)__calloc((listSize + resizeCount), sizeof(fiber**)), **old;
    for(Int i = 0; i < listSize; i++) {
        tmpfibers[i]=fiberAt(i);
    }

    listSize += resizeCount;
    old = fibers;
    fibers=tmpfibers;
    std::free(old);
    gc.addMemory(sizeof(fiber**) * resizeCount);
}

void decrease_fibers() {
    if(listSize > resizeCount && (listSize - fiberCount) >= resizeCount) {
        GUARD(fiberMutex)
        fiber** tmpfibers = (fiber**)__calloc((listSize - resizeCount), sizeof(fiber**)), **old;
        for(Int i = 0; i < listSize; i++) {
            tmpfibers[i]=fiberAt(i);
        }

        listSize -= resizeCount;
        old = fibers;
        fibers=tmpfibers;
        std::free(old);
        gc.freeMemory(sizeof(fiber**) * resizeCount);
    }
}

fiber* locateFiber(Int id) {
    for(Int i = 0; i < fiberCount; i++) {
        if(fiberAt(i) && fiberAt(i)->id == id) {
            return fiberAt(i);
        }
    }

    return nullptr;
}

void addFiber(fiber* fib) {
    GUARD(fiberMutex)
    for(Int i = 0; i < fiberCount; i++) {
        if(fiberAt(i) == NULL) {
            fiberAt(i) = fib;
            return;
        }
    }

    if(fiberCount >= listSize)
        increase_fibers();

    fibers.load(std::memory_order_relaxed)[fiberCount++] = fib;
}

fiber* fiber::makeFiber(native_string &name, Method* main) {
    // todo: check if space available before allocation
   fiber *fib = (fiber*)malloc(sizeof(fiber));

    try {
        fib->id = fibId++;
        fib->name.init();
        fib->name = name;
        fib->main = main;
        fib->cache = NULL;
        fib->pc = NULL;
        fib->pc = NULL;
        fib->state = FIB_CREATED;
        fib->exitVal = 0;
        fib->delayTime = -1;
        fib->wakeable = true;
        fib->finished = false;
        fib->locking = false;
        fib->marked = false;
        fib->attachedThread = NULL;
        fib->boundThread = NULL;
        fib->exceptionObject.object = NULL;
        fib->fiberObject.object = NULL;
        fib->dataStack = NULL;
        fib->registers = NULL;
        fib->callStack = NULL;
        fib->calls = -1;
        fib->current = NULL;
        fib->ptr = NULL;
        fib->stackLimit = internalStackSize;
        fib->registers = (double *) __calloc(REGISTER_SIZE, sizeof(double));
        fib->dataStack = (StackElement *) __calloc(internalStackSize, sizeof(StackElement));
        new (&fib->mut) recursive_mutex();

        if(internalStackSize - vm.manifest.threadLocals <= INITIAL_FRAME_SIZE) {
            fib->frameSize = internalStackSize - vm.manifest.threadLocals;
            fib->callStack = (Frame *) __calloc(internalStackSize - vm.manifest.threadLocals, sizeof(Frame));
        }
        else {
            fib->frameSize = INITIAL_FRAME_SIZE;
            fib->callStack = (Frame *) __calloc(INITIAL_FRAME_SIZE, sizeof(Frame));
        }

        fib->frameLimit = internalStackSize - vm.manifest.threadLocals;
        fib->fp = &fib->dataStack[vm.manifest.threadLocals];
        fib->sp = (&fib->dataStack[vm.manifest.threadLocals]) - 1;

        for(Int i = 0; i < vm.tlsInts.size(); i++) {
            fib->dataStack[vm.tlsInts.at(i).key].object =
                    gc.newObject(1, vm.tlsInts.at(i).value);
        }

        gc.addMemory(sizeof(Frame) * fib->frameSize);
        gc.addMemory(sizeof(StackElement) * internalStackSize);
        gc.addMemory(sizeof(double) * REGISTER_SIZE);
        addFiber(fib);
    } catch(Exception &e) {
        fib->free();
        std::free(fib);
        throw;
    }
    return fib;
}

fiber* fiber::getFiber(uInt id) {
    return locateFiber(id);
}

inline bool isFiberRunnble(fiber *fib, Int loggedTime, Thread *thread) {
    if(fib->state == FIB_SUSPENDED && fib->wakeable && fib->attachedThread == NULL) {
        if(fib->boundThread == thread || fib->boundThread == NULL) {
            if (fib->delayTime >= 0 && loggedTime >= fib->delayTime) {
                return true;
            } else if (fib->delayTime <= 0) {
                return true;
            }
        }
    }

    return false;
}

fiber* fiber::nextFiber(fiber *startingFiber, Thread *thread) {

    uInt loggedTime = NANO_TOMICRO(Clock::realTimeInNSecs());
    if(startingFiber != NULL) {
        for (Int i = 0; i < fiberCount; i++) {
            if (fiberAt(i) == startingFiber) {
                if ((i + 1) < fiberCount) {
                    for (Int j = i + 1; j < fiberCount; j++) {
                        if (fiberAt(j) && !fiberAt(j)->finished && !fiberAt(j)->locking && isFiberRunnble(fiberAt(j), loggedTime, thread)) {
                            return fiberAt(j);
                        }
                    }

                    break;
                } else break;
            }
        }
    }

    for(Int i = 0; i < fiberCount; i++) {
        fiber* fib = fiberAt(i);
       if(fib && isFiberRunnble(fib, loggedTime, thread))
           return fib;
    }

    return NULL;
}

void fiber::disposeFibers() {
    Int max = fiberCount;
    for(Int i = 0; i < max; i++) {
        fiber* fib = fiberAt(i);
        if(fib && fib->state == FIB_KILLED && fib->boundThread == NULL && fib->attachedThread == NULL && fib->finished) {
            if(fib->marked) {
                if(fib->state == FIB_KILLED && fib->boundThread == NULL && fib->attachedThread == NULL && fib->finished)
                {
                    fiberCount--;
                    fiberAt(i) = NULL;
                    fib->free();
                    std::free(fib);

                    decrease_fibers();
                }
                else fib->marked = false;
            } else fib->marked = true;
        }
    }
}

int fiber::suspend(uInt id) {
    fiber *fib = getFiber(id);
    int result = 0;

    if(fib) {
        GUARD(fib->mut)
        if(fib->state == FIB_SUSPENDED) {
            fib->setWakeable(false);
        } else if(fib->state == FIB_RUNNING) {
            if(fib->attachedThread) {
                fib->setWakeable(false);
                fib->setState(fib->attachedThread, FIB_SUSPENDED);
                fib->attachedThread->enableContextSwitch(true);
            } else {
                result = 2;
            }
        } else {
            result = 1;
        }
    }

    return result;
}

int fiber::unsuspend(uInt id) {
    fiber *fib = getFiber(id);
    int result = 0;

    if(fib) {
        GUARD(fib->mut)
        if(fib->state == FIB_SUSPENDED) {
            fib->setWakeable(true);
        } else {
            result = 1;
        }
    }

    return result;
}

int fiber::kill(uInt id) {
    fiber *fib = getFiber(id);
    int result = 0;

    if(fib) {
        GUARD(fib->mut)
        if(fib->state == FIB_SUSPENDED) {
            fib->setState(NULL, FIB_KILLED);
        } else if(fib->state == FIB_RUNNING) {
            if(fib->attachedThread) {
                fib->setState(NULL, FIB_KILLED);
                fib->attachedThread->enableContextSwitch(true);
            } else {
                result = 2;
            }
        } else {
            fib->setState(NULL, FIB_KILLED);
        }
    }

    return result;
}

void fiber::free() {
    GUARD(fiberMutex)
    gc.reconcileLocks(this);

    bind(NULL);
    if(dataStack != NULL) {
        gc.freeMemory(sizeof(StackElement) * stackLimit);
        StackElement *p = dataStack;
        for(size_t i = 0; i < stackLimit; i++)
        {
            if(p->object.object) {
                DEC_REF(p->object.object);
                p->object.object=NULL;
            }
            p++;
        }

        std::free(this->dataStack); dataStack = NULL;
    }

    if(registers != NULL) {
        gc.freeMemory(sizeof(double) * REGISTER_SIZE);
        std::free(registers); registers = NULL;
    }

    if(callStack != NULL) {
        gc.freeMemory(sizeof(Frame) * frameSize);
        std::free(callStack); callStack = NULL;
    }

    fiberObject=(SharpObject*)NULL;
    exceptionObject=(SharpObject*)NULL;
    fp = NULL;
    sp = NULL;
    name.free();
    id = -1;
}

int fiber::getState() {
    GUARD(mut)
    auto result = (Int)state;
    return result;
}

void fiber::setState(Thread *thread, fiber_state newState, Int delay) {
    GUARD(mut)

    switch(newState) {
        case FIB_RUNNING:
            thread->lastRanMicros = NANO_TOMICRO(Clock::realTimeInNSecs());
            state = newState;
            delayTime = -1;
            break;
        case FIB_SUSPENDED:
            delayTime = delay;
            state = newState;
            break;
        case FIB_KILLED:
            bind(NULL);
            state = newState;
            delayTime = -1;
            break;
    }
}

void fiber::setWakeable(bool enable) {
    GUARD(mut)
    wakeable = enable;
}

Thread *fiber::getAttachedThread() {
    GUARD(mut)
    auto result = attachedThread;
    return result;
}

Thread *fiber::getBoundThread() {
    GUARD(mut)
    auto result = boundThread;
    return result;
}

void fiber::setAttachedThread(Thread *thread) {
    GUARD(mut)
    attachedThread = thread;
}

void fiber::delay(Int time) {
    GUARD(mut)
    if(time < 0)
        time = -1;

    attachedThread->enableContextSwitch(true);
    attachedThread->contextSwitching = true;
    setState(thread_self, FIB_SUSPENDED, NANO_TOMICRO(Clock::realTimeInNSecs()) + time);
}

bool fiber::safeStart(Thread *thread) {
    GUARD(mut)
    if(state == FIB_SUSPENDED && attachedThread == NULL) {
        attachedThread = (thread);
        setState(thread, FIB_RUNNING);
    } else {
        return false;
    }

    return true;
}

int fiber::bind(Thread *thread) {
    GUARD(mut)

    if(thread != NULL) {
        std::lock_guard<recursive_mutex> guard2(thread->mutex);
        if(thread->state != THREAD_KILLED || !hasSignal(thread->signal, tsig_kill)) {
            boundThread = thread;
            thread->boundFibers++;
            return 0;
        }
    } else {
        if(boundThread) {
            std::lock_guard<recursive_mutex> guard2(boundThread->mutex);
            boundThread->boundFibers--;

        }

        boundThread = NULL;
        return 0;
    }

    return 1;
}

Int fiber::boundFiberCount(Thread *thread) {
    if(thread != NULL) {
        GUARD(thread->mutex);
        return thread->boundFibers;
    }
    return 0;
}

void fiber::killBoundFibers(Thread *thread) {
    for(Int i = 0; i < fiberCount; i++) {
        fiber *fib = fiberAt(i);
        if(fib && fib->getBoundThread() == thread && fib->state != FIB_KILLED && fib != thread->this_fiber) {
            kill(fib->id);
        }
    }
}

void fiber::growFrame() {
    GUARD(mut)

    if(frameSize + FRAME_GROW_SIZE < frameLimit) {
        callStack = (Frame *) __realloc(callStack, sizeof(Frame) * (frameSize + FRAME_GROW_SIZE),
                                        sizeof(Frame) * frameSize);
        frameSize += FRAME_GROW_SIZE;
        gc.addMemory(sizeof(Frame) * FRAME_GROW_SIZE);
    }
    else {
        callStack = (Frame *) __realloc(callStack, sizeof(Frame) * (frameLimit), sizeof(Frame) * frameSize);
        gc.addMemory(sizeof(Frame) * (frameLimit - frameSize));
        frameSize = frameLimit;
    }
}


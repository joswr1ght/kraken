#include "CalModule.h"
#include <stdio.h>

CalModule::CalModule(CALcontext* ctx) :
    mCtx(ctx),
    mLoaded(false),
    mRunning(false)
{
}

CalModule::~CalModule()
{
    Cleanup();
}

void CalModule::Cleanup()
{
    if (mLoaded) {
        calModuleUnload(*mCtx, mModule);

        streamdict_t::iterator it = mStreams.begin();
        while (it!=mStreams.end()) {
            calCtxReleaseMem(*mCtx, (*it).second.mem);
            CalDevice::unrefResource((*it).second.pRes);
            it++;
        }
        mStreams.clear();
    }
    mLoaded = false;
}

bool CalModule::Load(CALimage &image)
{
    if (mLoaded) Cleanup();

    if (calModuleLoad(&mModule, *mCtx, image) != CAL_RESULT_OK) {
        printf("Error calModuleLoad() - %s.\n", calGetErrorString() );
        return false;
    }

    if (calModuleGetEntry(&mEntry, *mCtx, mModule, "main") != CAL_RESULT_OK) {
        calModuleUnload(*mCtx, mModule);
        printf("Error calModuleGetEntry() - %s.\n", calGetErrorString() );
        return false;
    }

    mLoaded = true;
    return true;
}

CALmem* CalModule::Bind(const char* name, CALresource* pRes)
{
    if (!mLoaded) return NULL;

    stream_t stream;
    stream.pRes = pRes;

    std::string skey(name);

    CALmem mem = 0;
    if (calCtxGetMem(&mem, *mCtx, *pRes) != CAL_RESULT_OK) {
        printf("Error calCtxGetMem (%s).\n", calGetErrorString());
        return NULL;
    }

    streamdict_t::iterator it = mStreams.find(skey);
    if (it!=mStreams.end()) {
        /* Already exists */
        calCtxReleaseMem(*mCtx, (*it).second.mem);

        if (calCtxSetMem(*mCtx, (*it).second.name, mem) != CAL_RESULT_OK) {
            calCtxReleaseMem(*mCtx, mem);
            mStreams.erase(it);
            printf("Error calCtxSetMem (%s).\n", calGetErrorString());
            return NULL;
        }

        (*it).second.mem = mem;

        CalDevice::unrefResource((*it).second.pRes);
    } else {
        if (calModuleGetName(&stream.name, *mCtx, mModule, name) != CAL_RESULT_OK) {
            printf("Error calModuleGetName(%s) - %s.\n", name, calGetErrorString() );
            calCtxReleaseMem(*mCtx, mem);
            return NULL;
        }

        if (calCtxSetMem(*mCtx, stream.name, mem) != CAL_RESULT_OK) {
            calCtxReleaseMem(*mCtx, mem);
            printf("Error calCtxSetMem (%s).\n", calGetErrorString());
            return NULL;
        }
        stream.mem = mem;
        mStreams[skey] = stream;
    }

    CalDevice::refResource(pRes);
    return &mStreams[skey].mem;
}

bool CalModule::Exec(CALdomain &dom, int groupsize)
{
    if (mRunning) return false;

    if (groupsize) {
        CALprogramGrid pg;
        pg.func = mEntry;
        pg.flags = 0;
        pg.gridBlock.width = groupsize;
        pg.gridBlock.height = 1;
        pg.gridBlock.depth = 1;
        pg.gridSize.width = (dom.width * dom.height + groupsize - 1) / groupsize;
        pg.gridSize.height = 1;
        pg.gridSize.depth = 1;
        if (calCtxRunProgramGrid(&mEvent, *mCtx, &pg) != CAL_RESULT_OK) {
            printf("Error calCtxRunProgram() - %s.\n", calGetErrorString() );
            return false;
        }
    } else {
        if (calCtxRunProgram(&mEvent, *mCtx, mEntry, &dom) != CAL_RESULT_OK) {
            printf("Error calCtxRunProgram() - %s.\n", calGetErrorString() );
            return false;
        }
    }

    mRunning = true;
    return true;
}

bool CalModule::Finished()
{
    if (!mRunning) return true;
    if (calCtxIsEventDone(*mCtx, mEvent)==CAL_RESULT_PENDING) return false;
    mRunning = false;
    return true;
}


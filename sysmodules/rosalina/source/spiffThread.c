#include <3ds.h>
#include "spiffThread.h"
#include "draw.h"
#include "menu.h"
#include "memory.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <inttypes.h>
#include "pmdbgext.h"

#define SPIFF_COMBO (KEY_LEFT | KEY_START)
#define SPIFF_THREAD_STACK_SIZE 0x1000
#define TIMELIMIT_DEFAULT 900
#define ADDITIONAL_SECONDS 600

static MyThread spiffThread;
static u8 CTR_ALIGN(8) spiffThreadStack[SPIFF_THREAD_STACK_SIZE];
u64 timeLimit = TIMELIMIT_DEFAULT;
u64 timeBonus = 0;

MyThread *spiffCreateThread(void)
{
    if (R_FAILED(MyThread_Create(&spiffThread, spiffThreadMain, spiffThreadStack, SPIFF_THREAD_STACK_SIZE, 0x3F, CORE_SYSTEM)))
        svcBreak(USERBREAK_PANIC);
    return &spiffThread;
}

u64 http_download(IFile* file, const char *url)
{
    char log[256];
    u32 logsize = 0;
    u64 remoteLimit = timeLimit;

    logsize = sprintf(log,"Starting Downloading...");
    logMsg(file, log, logsize);

    Result ret = 0;
    httpcContext context;
	
    u32 statuscode = 0;
    u32 contentsize = 0, readsize = 0, size = 0;
    u8 buf[128];

    logsize = sprintf(log,"Downloading %s", url);
    logMsg(file, log, logsize);

    ret = httpcOpenContext(&context, HTTPC_METHOD_GET, url, 1);
    logsize = sprintf(log,"return from httpcOpenContext: %" PRId32, ret);
    logMsg(file, log, logsize);

    // This disables SSL cert verification, so https:// will be usable
    ret = httpcSetSSLOpt(&context, SSLCOPT_DisableVerify);
    logsize = sprintf(log,"return from httpcSetSSLOpt: %" PRId32, ret);
    logMsg(file, log, logsize);

    // Enable Keep-Alive connections
    ret = httpcSetKeepAlive(&context, HTTPC_KEEPALIVE_ENABLED);
    logsize = sprintf(log,"return from httpcSetKeepAlive: %" PRId32, ret);
    logMsg(file, log, logsize);

    // Set a User-Agent header so websites can identify your application
    ret = httpcAddRequestHeaderField(&context, "User-Agent", "httpc-example/1.0.0");
    logsize = sprintf(log,"return from httpcAddRequestHeaderField: %" PRId32, ret);
    logMsg(file, log, logsize);

    // Tell the server we can support Keep-Alive connections.
    // This will delay connection teardown momentarily (typically 5s)
    // in case there is another request made to the same server.
    ret = httpcAddRequestHeaderField(&context, "Connection", "Keep-Alive");
    logsize= sprintf(log,"return from httpcAddRequestHeaderField: %" PRId32, ret);
    logMsg(file, log, logsize);

    ret = httpcBeginRequest(&context);
    logsize= sprintf(log,"return from httpcBeginRequest: %" PRId32, ret);
    logMsg(file, log, logsize);
    if (ret != 0)
    {
        httpcCloseContext(&context);
        return TIMELIMIT_DEFAULT;
    }

    ret = httpcGetResponseStatusCode(&context, &statuscode);
    logsize= sprintf(log,"return from httpcGetResponseStatusCode: %" PRId32, ret);
    logMsg(file, log, logsize);
    if (ret != 0)
    {
        httpcCloseContext(&context);
        return TIMELIMIT_DEFAULT;
    }
    logsize= sprintf(log,"return from statuscode: %" PRId32, statuscode);
    logMsg(file, log, logsize);
        
       
    if (statuscode != 200) 
    {
        httpcCloseContext(&context);
        return TIMELIMIT_DEFAULT;
    }

    // This relies on an optional Content-Length header and may be 0
    ret = httpcGetDownloadSizeState(&context, NULL, &contentsize);
    logsize= sprintf(log,"return from httpcGetDownloadSizeState: %" PRId32, ret);
    logMsg(file, log, logsize);
    if (ret != 0)
    {
        httpcCloseContext(&context);
        return TIMELIMIT_DEFAULT;
    }

    logsize= sprintf(log,"reported size: %" PRId32, contentsize);
    logMsg(file, log, logsize);


    do
    {
        // This download loop resizes the buffer as data is read.
        ret = httpcDownloadData(&context, buf + size, 0xff, &readsize);
        logsize= sprintf(log,"return from httpcDownloadData: %" PRId32, ret);
        logMsg(file, log, logsize);
		logsize= sprintf(log,"readSize: %ld", readsize);
        logMsg(file, log, logsize);
        logsize= sprintf(log,"buf: %s", buf);
        logMsg(file, log, logsize);
        size += readsize;
       
    } while (ret == (s32)HTTPC_RESULTCODE_DOWNLOADPENDING);

    if (ret != 0)
    {
        httpcCloseContext(&context);
        return TIMELIMIT_DEFAULT;
    }

    remoteLimit = atoi((char *)buf);
    if(remoteLimit == 0) remoteLimit = TIMELIMIT_DEFAULT;

    logsize= sprintf(log,"download size: %" PRId32, size);
    logMsg(file, log, logsize);

    httpcCloseContext(&context);

    return remoteLimit;
}

void logMsg(IFile *file, char *msg, u32 logSize)
{
    if (!menuShouldExit) 
    {
        // IFile file;
        u64 total = 0;
        char logmsg[256];
        // FS_ArchiveID archiveId = ARCHIVE_SDMC;
        // IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/spiffLog.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE);

        char dateTimeStr[32];
        u64 timeNow = osGetTime();
        dateTimeToString(dateTimeStr, timeNow, true);
        logSize = sprintf(logmsg, "%s - %s\n", dateTimeStr, msg);

        IFile_Write(file, &total, logmsg, logSize, 0);
        // IFile_Close(file);
    }
}

u64 logTime(bool write)
{
    if (!menuShouldExit) 
    {
        IFile file;
        char dateTimeStr[32], timeFileName[64];
        u64 total, fileSize = 0;
        u64 timeNow = osGetTime();
        FS_ArchiveID archiveId = ARCHIVE_SDMC;

        dateTimeToFilename(dateTimeStr, timeNow);
        sprintf(timeFileName, "/luma/%s.txt", dateTimeStr);

        IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, timeFileName), FS_OPEN_CREATE | FS_OPEN_WRITE);
        IFile_GetSize(&file, &fileSize);
        file.pos = fileSize;
        if (write)
        {
            IFile_Write(&file, &total, ".", 1, 0);
        }
        IFile_Close(&file);

        return file.size;
    }
    return 0;
}

int dateTimeToFilename(char *out, u64 msSince1900)
{
    // Conversion code adapted from https://stackoverflow.com/questions/21593692/convert-unix-timestamp-to-date-without-system-libs
    // (original author @gnif under CC-BY-SA 4.0)
    u32 seconds, minutes, hours, days, year, month;
    u64 milliseconds = msSince1900;
    seconds = milliseconds / 1000;
    milliseconds %= 1000;
    minutes = seconds / 60;
    seconds %= 60;
    hours = minutes / 60;
    minutes %= 60;
    days = hours / 24;
    hours %= 24;

    year = 1900; // osGetTime starts in 1900

    while (true)
    {
        bool leapYear = (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
        u16 daysInYear = leapYear ? 366 : 365;
        if (days >= daysInYear)
        {
            days -= daysInYear;
            ++year;
        }
        else
        {
            static const u8 daysInMonth[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
            for (month = 0; month < 12; ++month)
            {
                u8 dim = daysInMonth[month];

                if (month == 1 && leapYear)
                    ++dim;

                if (days >= dim)
                    days -= dim;
                else
                    break;
            }
            break;
        }
    }
    days++;
    month++;

    return sprintf(out, "%04lu-%02lu-%02lu", year, month, days);
}

void spiffMenuEnter()
{
    Draw_Lock();

    svcKernelSetState(0x10000, 2 | 1);
    svcSleepThread(5 * 1000 * 100LL);
    if (R_FAILED(Draw_AllocateFramebufferCache(FB_BOTTOM_SIZE)))
    {
        svcKernelSetState(0x10000, 2 | 1);
        svcSleepThread(5 * 1000 * 100LL);
    }
    else
    {
        Draw_SetupFramebuffer();
    }

    Draw_Unlock();
}

void spiffMenu(IFile *file)
{
    Draw_Lock();
    Draw_ClearFramebuffer();
    Draw_FlushFramebuffer();
    Draw_Unlock();
    u64 timeFileSize = logTime(false);
    int count = 0;
    char log[256];
    u32 logSize;

    do
    {
        Draw_Lock();
        Draw_DrawString(10, 10, COLOR_TITLE, "Time is up!");

        u32 posY = Draw_DrawString(10, 30, COLOR_WHITE, "To add 10 more minutes:") + SPACING_Y;

        posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Press Left and Start");
        // posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "Networking code & basic GDB functionality by Stary");
        // posY = Draw_DrawString(10, posY + SPACING_Y, COLOR_WHITE, "InputRedirection by Stary (PoC by ShinyQuagsire)");

        Draw_FlushFramebuffer();
        Draw_Unlock();
        if ((HID_PAD & SPIFF_COMBO) == (SPIFF_COMBO - 0x20000000))
        {
            timeBonus += ADDITIONAL_SECONDS;
            count = 100;
        }
        if (count % 100 == 0)
        {
            timeFileSize = logTime(false);
            
            logSize = sprintf(log, "timeFileSize: %lld timeLimit: %lld", timeFileSize, timeLimit);
            logMsg(file, log, logSize);
        }
        if (count>500) {
            count = 0;
            
            u64 titleId = getCurrentTitleID();
            logSize = sprintf(log, "http://xxxx.com/3ds/?title=%016llx",titleId);
            
            timeLimit=http_download(file, log);
        }
        count++;
        svcSleepThread(10 * 1000 * 1000);
        
        
    } while ((timeFileSize > timeLimit + timeBonus) && !menuShouldExit);
}

void spiffMenuExit()
{
    svcSleepThread(50 * 1000 * 1000);

    Draw_Lock();
    Draw_RestoreFramebuffer();
    Draw_FreeFramebufferCache();
    svcKernelSetState(0x10000, 2 | 1);

    Draw_Unlock();
}

u64 getCurrentTitleID()
{
    FS_ProgramInfo programInfo;
    u32 pid;
    u32 launchFlags;
    Result res = PMDBG_GetCurrentAppInfo(&programInfo, &pid, &launchFlags);
    if (R_FAILED(res)) {
        return 0;
    }

    return programInfo.programId;
}

void spiffThreadMain(void)
{
    char log[256];
    u64 fileSize = 0;
    u32 logSize = 0;
    int count = 0;

    IFile file;
    FS_ArchiveID archiveId = ARCHIVE_SDMC;
    IFile_Open(&file, archiveId, fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, "/luma/spiffLog.txt"), FS_OPEN_CREATE | FS_OPEN_WRITE);

    logMsg(&file, log, logSize);

    IFile_GetSize(&file, &fileSize);
    file.pos = fileSize;
    fileSize = 0;

    logSize = sprintf(log, "Starting...\n");
    logMsg(&file, log, logSize);

    httpcInit(0);

    while (!preTerminationRequested)
    {
        if ((HID_PAD & SPIFF_COMBO) == (SPIFF_COMBO - 0x20000000))
        {
            // NEWS_AddNotification(u"test", 4, u"test test", 9, NULL, 0, false);
            u64 titleId = getCurrentTitleID();
            logSize = sprintf(log, "http://xxxx.com/3ds/?title=%016llx",titleId);
            logMsg(&file, log, logSize);
            
            timeLimit=http_download(&file, log);
            logSize = sprintf(log, "timeLimit:  %lld", timeLimit);
            logMsg(&file, log, logSize);
            svcSleepThread(2000 * 1000 * 1000LL);
        }

        count++;
        if (count % 10 == 0)
        {
            if(!menuShouldExit)
            {
                fileSize = logTime(true);
                if (fileSize > timeLimit + timeBonus)
                {
                    // srvPublishToSubscriber(0x202, 0);
                    spiffMenuEnter();
                    spiffMenu(&file);
                    spiffMenuExit();
                }
            }
        }

        if(count>50) 
        {
            logSize = sprintf(log, "getting current title...");
            logMsg(&file, log, logSize);
            u64 titleId = getCurrentTitleID();
            logSize = sprintf(log, "http://xxxx.com/3ds/?title=%016llx",titleId);
            logMsg(&file, log, logSize);
            
            timeLimit=http_download(&file, log);
            logSize = sprintf(log, "timeLimit:  %lld", timeLimit);
            logMsg(&file, log, logSize);
            count = 0;
        }

        //logSize = sprintf(log, "TimeFile Size: %lld HIDPAD: %ld TimeLimit: %lld", fileSize, HID_PAD, timeLimit);
        //logMsg(&file, log, logSize);

        svcSleepThread(100 * 1000 * 1000LL);
    }

    logSize = sprintf(log, "Ending...");
    logMsg(&file, log, logSize);

    IFile_Close(&file);
    httpcExit();
}

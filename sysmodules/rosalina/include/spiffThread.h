#pragma once

#include <3ds/types.h>
#include "MyThread.h"
#include "ifile.h"

MyThread *spiffCreateThread(void);

void spiffThreadMain(void);

int dateTimeToFilename(char *out, u64 msSince1900);

void logMsg(IFile *file, char *msg, u32 logSize);

u64 getCurrentTitleID();
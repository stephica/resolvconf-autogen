//
//  DRHelper.h
//  gw-monitor
//
//  Created by Zhang Yi on 13-3-23.
//  Copyright (c) 2013年 Zhang Yi.
//

#ifndef gw_monitor_DRHelper_h
#define gw_monitor_DRHelper_h

#include <sys/types.h>
#include <sys/sysctl.h>
#include <unistd.h>
#include <signal.h>

typedef struct kinfo_proc kinfo_proc;
static inline int GetBSDProcessList(kinfo_proc **procList, size_t *procCount)
// Returns a list of all BSD processes on the system.  This routine
// allocates the list and puts it in *procList and a count of the
// number of entries in *procCount.  You are responsible for freeing
// this list (use "free" from System framework).
// On success, the function returns 0.
// On error, the function returns a BSD errno value.
{
    int                 err;
    kinfo_proc *        result;
    bool                done;
    static const int    name[] = { CTL_KERN, KERN_PROC, KERN_PROC_ALL, 0 };
    // Declaring name as const requires us to cast it when passing it to
    // sysctl because the prototype doesn't include the const modifier.
    size_t              length;

    assert( procList != NULL);
    assert(*procList == NULL);
    assert(procCount != NULL);

    *procCount = 0;

    // We start by calling sysctl with result == NULL and length == 0.
    // That will succeed, and set length to the appropriate length.
    // We then allocate a buffer of that size and call sysctl again
    // with that buffer.  If that succeeds, we're done.  If that fails
    // with ENOMEM, we have to throw away our buffer and loop.  Note
    // that the loop causes use to call sysctl with NULL again; this
    // is necessary because the ENOMEM failure case sets length to
    // the amount of data returned, not the amount of data that
    // could have been returned.

    result = NULL;
    done = false;
    do {
        assert(result == NULL);

        // Call sysctl with a NULL buffer.

        length = 0;
        err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                     NULL, &length,
                     NULL, 0);
        if (err == -1) {
            err = errno;
        }

        // Allocate an appropriately sized buffer based on the results
        // from the previous call.

        if (err == 0) {
            result = malloc(length);
            if (result == NULL) {
                err = ENOMEM;
            }
        }

        // Call sysctl again with the new buffer.  If we get an ENOMEM
        // error, toss away our buffer and start again.

        if (err == 0) {
            err = sysctl( (int *) name, (sizeof(name) / sizeof(*name)) - 1,
                         result, &length,
                         NULL, 0);
            if (err == -1) {
                err = errno;
            }
            if (err == 0) {
                done = true;
            } else if (err == ENOMEM) {
                assert(result != NULL);
                free(result);
                result = NULL;
                err = 0;
            }
        }
    } while (err == 0 && ! done);

    // Clean up and establish post conditions.

    if (err != 0 && result != NULL) {
        free(result);
        result = NULL;
    }
    *procList = result;
    if (err == 0) {
        *procCount = length / sizeof(kinfo_proc);
    }

    assert( (err == 0) == (*procList != NULL) );
    
    return err;
}

static inline NSDictionary* GetInfoForPID(pid_t pid)
{
    NSDictionary *ret = nil;
    ProcessSerialNumber psn = { kNoProcess, kNoProcess };
    if (GetProcessForPID(pid, &psn) == noErr) {
        CFDictionaryRef cfDict = ProcessInformationCopyDictionary(&psn,kProcessDictionaryIncludeAllInformationMask);
        ret = [NSDictionary dictionaryWithDictionary:(__bridge NSDictionary *)cfDict];
        CFRelease(cfDict);
    }
    return ret;
}

static inline NSArray* GetBSDProcessListAsNSArray()
{
    NSMutableArray *ret = [NSMutableArray arrayWithCapacity:1];
    kinfo_proc *mylist = NULL;
    size_t mycount = 0;
    GetBSDProcessList(&mylist, &mycount);
    int k;
    for(k = 0; k < mycount; k++) {
        kinfo_proc *proc = NULL;
        proc = &mylist[k];
        NSString *fullName = [GetInfoForPID(proc->kp_proc.p_pid) objectForKey:(id)kCFBundleNameKey];
        if (fullName == nil) fullName = [NSString stringWithFormat:@"%s",proc->kp_proc.p_comm];
            [ret addObject:[NSDictionary dictionaryWithObjectsAndKeys:
                            fullName,@"pname",
                            [NSNumber numberWithInteger:proc->kp_proc.p_pid],@"pid",
                            [NSNumber numberWithInteger:proc->kp_eproc.e_ucred.cr_uid],@"uid",
                            nil]];
    }
    free(mylist);
    return ret;
}

static inline pid_t GetPidForProcessWithName(NSString *pname)
{
    NSArray *list = GetBSDProcessListAsNSArray();
    for (NSDictionary *info in list) {
        if ([pname isEqualToString:[info objectForKey:@"pname"]]) {
            return (pid_t)[[info objectForKey:@"pid"] integerValue];
        }
    }

    return -1;
}

#endif

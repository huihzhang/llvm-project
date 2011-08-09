//===-- DNBArchImplI386.h ---------------------------------------*- C++ -*-===//
//
//                     The LLVM Compiler Infrastructure
//
// This file is distributed under the University of Illinois Open Source
// License. See LICENSE.TXT for details.
//
//===----------------------------------------------------------------------===//
//
//  Created by Greg Clayton on 6/25/07.
//
//===----------------------------------------------------------------------===//

#ifndef __DNBArchImplI386_h__
#define __DNBArchImplI386_h__

#if defined (__i386__) || defined (__x86_64__)

#include "DNBArch.h"
#include "../HasAVX.h"
#include "MachRegisterStatesI386.h"

class MachThread;

class DNBArchImplI386 : public DNBArchProtocol
{
public:
    DNBArchImplI386(MachThread *thread) :
        m_thread(thread),
        m_state()
    {
    }
    virtual ~DNBArchImplI386()
    {
    }
    
    static  void            Initialize();

    virtual bool            GetRegisterValue(int set, int reg, DNBRegisterValue *value);
    virtual bool            SetRegisterValue(int set, int reg, const DNBRegisterValue *value);
    virtual nub_size_t      GetRegisterContext (void *buf, nub_size_t buf_len);
    virtual nub_size_t      SetRegisterContext (const void *buf, nub_size_t buf_len);
    virtual kern_return_t   GetRegisterState  (int set, bool force);
    virtual kern_return_t   SetRegisterState  (int set);
    virtual bool            RegisterSetStateIsValid (int set) const;

    virtual uint64_t        GetPC(uint64_t failValue);    // Get program counter
    virtual kern_return_t   SetPC(uint64_t value);
    virtual uint64_t        GetSP(uint64_t failValue);    // Get stack pointer
    virtual void            ThreadWillResume();
    virtual bool            ThreadDidStop();
    virtual bool            NotifyException(MachException::Data& exc);

protected:
    kern_return_t           EnableHardwareSingleStep (bool enable);

    typedef __i386_thread_state_t GPR;
    typedef __i386_float_state_t FPU;
    typedef __i386_exception_state_t EXC;
    typedef __i386_avx_state_t AVX;

    static const DNBRegisterInfo g_gpr_registers[];
    static const DNBRegisterInfo g_fpu_registers_no_avx[];
    static const DNBRegisterInfo g_fpu_registers_avx[];
    static const DNBRegisterInfo g_exc_registers[];
    static const DNBRegisterSetInfo g_reg_sets_no_avx[];
    static const DNBRegisterSetInfo g_reg_sets_avx[];
    static const size_t k_num_gpr_registers;
    static const size_t k_num_fpu_registers_no_avx;
    static const size_t k_num_fpu_registers_avx;
    static const size_t k_num_exc_registers;
    static const size_t k_num_all_registers_no_avx;
    static const size_t k_num_all_registers_avx;
    static const size_t k_num_register_sets;

    typedef enum RegisterSetTag
    {
        e_regSetALL = REGISTER_SET_ALL,
        e_regSetGPR,
        e_regSetFPU,
        e_regSetEXC,
        kNumRegisterSets
    } RegisterSet;

    typedef enum RegisterSetWordSizeTag
    {
        e_regSetWordSizeGPR = sizeof(GPR) / sizeof(int),
        e_regSetWordSizeFPR = sizeof(FPU) / sizeof(int),
        e_regSetWordSizeEXC = sizeof(EXC) / sizeof(int),
        e_regSetWordSizeAVX = sizeof(AVX) / sizeof(int)
    } RegisterSetWordSize;

    enum
    {
        Read = 0,
        Write = 1,
        kNumErrors = 2
    };

    struct Context
    {
        __i386_thread_state_t       gpr;
        union {
            __i386_float_state_t    no_avx;
            __i386_avx_state_t      avx;
        } fpu;
        __i386_exception_state_t    exc;
    };

    struct State
    {
        Context context;
        kern_return_t gpr_errs[2];    // Read/Write errors
        kern_return_t fpu_errs[2];    // Read/Write errors
        kern_return_t exc_errs[2];    // Read/Write errors
        
        State()
        {
            uint32_t i;
            for (i=0; i<kNumErrors; i++)
            {
                gpr_errs[i] = -1;
                fpu_errs[i] = -1;
                exc_errs[i] = -1;
            }
        }
        void InvalidateAllRegisterStates()
        {
            SetError (e_regSetALL, Read, -1);
        }
        kern_return_t GetError (int flavor, uint32_t err_idx) const
        {
            if (err_idx < kNumErrors)
            {
                switch (flavor)
                {
                // When getting all errors, just OR all values together to see if
                // we got any kind of error.
                case e_regSetALL:    return gpr_errs[err_idx] |
                                            fpu_errs[err_idx] |
                                            exc_errs[err_idx];
                case e_regSetGPR:    return gpr_errs[err_idx];
                case e_regSetFPU:    return fpu_errs[err_idx];
                case e_regSetEXC:    return exc_errs[err_idx];
                default: break;
                }
            }
            return -1;
        }
        bool SetError (int flavor, uint32_t err_idx, kern_return_t err)
        {
            if (err_idx < kNumErrors)
            {
                switch (flavor)
                {
                case e_regSetALL:
                    gpr_errs[err_idx] =
                    fpu_errs[err_idx] =
                    exc_errs[err_idx] = err;
                    return true;

                case e_regSetGPR:
                    gpr_errs[err_idx] = err;
                    return true;

                case e_regSetFPU:
                    fpu_errs[err_idx] = err;
                    return true;

                case e_regSetEXC:
                    exc_errs[err_idx] = err;
                    return true;

                default: break;
                }
            }
            return false;
        }
        bool RegsAreValid (int flavor) const
        {
            return GetError(flavor, Read) == KERN_SUCCESS;
        }
    };

    kern_return_t GetGPRState (bool force);
    kern_return_t GetFPUState (bool force);
    kern_return_t GetEXCState (bool force);

    kern_return_t SetGPRState ();
    kern_return_t SetFPUState ();
    kern_return_t SetEXCState ();

    static DNBArchProtocol *
    Create (MachThread *thread);
    
    static const uint8_t * const
    SoftwareBreakpointOpcode (nub_size_t byte_size);

    static const DNBRegisterSetInfo *
    GetRegisterSetInfo(nub_size_t *num_reg_sets);
    
    static bool
    CPUHasAVX()
    {
#if 0
        if (s_has_avx == kAVXUnknown)
            s_has_avx = (::HasAVX() ? kAVXPresent : kAVXNotPresent);
        
        return (s_has_avx == kAVXPresent);
#else
        // ::HasAVX() will cause this code to crash because the 
        // assembly function doesn't backup and restore the registers
        // it uses. Until this is fixed, AVX will be disabled.
        return 0; 
#endif
    }

    MachThread *m_thread;
    State       m_state;
            
    static enum AVXPresence {
        kAVXPresent,
        kAVXNotPresent,
        kAVXUnknown
    } s_has_avx;
};

#endif    // #if defined (__i386__) || defined (__x86_64__)
#endif    // #ifndef __DNBArchImplI386_h__

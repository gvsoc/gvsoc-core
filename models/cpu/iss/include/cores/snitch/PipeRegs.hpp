/*
 * Copyright (C) 2020 GreenWaves Technologies, SAS, ETH Zurich and
 *                    University of Bologna
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/* 
 * Authors: Germain Haugou, GreenWaves Technologies (germain.haugou@greenwaves-technologies.com)
 *          Kexin Li, ETH Zurich (likexi@ethz.ch)
 */
 
#pragma once

#include <cpu/iss/include/types.hpp>

class FpuOp
{
public:
    // PC of past instruction.
    iss_reg_t pc;
    // Latency of the instruction.
    int insn_latency;
    // Start time of the instruction
    int start_timestamp = 0;
    // Finish time of the instruction
    int end_timestamp = 0;
    // Output register index of past instruction.
    int rd = -1;
};


// Instruction stored in FIFO with depth of 3 should have latency of 3 cycles originally.
class FIFODepth3
{
public:

    FpuOp FpuPipe[3];

    bool check_dependency(int index, int rs1, int rs2, int rs3)
    {
        bool dependency = false;
        if (FpuPipe[index].rd >= 0)
        {
            dependency |= (rs1 == FpuPipe[index].rd);
            dependency |= (rs2 == FpuPipe[index].rd);
            dependency |= (rs3 == FpuPipe[index].rd);
        }
        return dependency;
    }

    int get_latency(int current_timestamp, int insn_latency, int rs1, int rs2, int rs3)
    {
        int temp_latency = 0;
        int past_latency = 0;
        int latency = insn_latency;

        for (int i=0; i<3; i++)
        {
            if (!check_dependency(i, rs1, rs2, rs3))
            {
                past_latency = FpuPipe[i].insn_latency;
                if (insn_latency - past_latency > 0)
                {
                    temp_latency =  current_timestamp - (FpuPipe[i].end_timestamp+1) 
                                    + (insn_latency - past_latency);
                }
                else
                {
                    temp_latency =  current_timestamp - (FpuPipe[i].end_timestamp+1);
                }
            }
            else
            {
                temp_latency = insn_latency;
            }

            if (temp_latency < latency)
            {
                latency = temp_latency;
            }
        }

        return latency;
    }

    void enqueue(FpuOp value) 
    {
        // Find the entry with the smallest timestamp and replace it with the new one
        int index = 0;
        int end_timestamp = FpuPipe[0].end_timestamp;

        for (int i=0; i<3; i++)
        {
            if (FpuPipe[i].end_timestamp < end_timestamp)
            {
                index = i;
            }
        }

        FpuPipe[index] = value;
    }
};


// Instruction stored in FIFO with depth of 2 should have latency of 2 cycles originally.
class FIFODepth2
{
public:

    FpuOp FpuPipe[2];

    bool check_dependency(int index, int rs1, int rs2, int rs3)
    {
        bool dependency = false;
        if (FpuPipe[index].rd >= 0)
        {
            dependency |= (rs1 == FpuPipe[index].rd);
            dependency |= (rs2 == FpuPipe[index].rd);
            dependency |= (rs3 == FpuPipe[index].rd);
        }
        return dependency;
    }

    int get_latency(int current_timestamp, int insn_latency, int rs1, int rs2, int rs3)
    {
        int temp_latency = 0;
        int past_latency = 0;
        int latency = insn_latency;

        for (int i=0; i<2; i++)
        {
            if (!check_dependency(i, rs1, rs2, rs3))
            {
                past_latency = FpuPipe[i].insn_latency;
                if (insn_latency - past_latency > 0)
                {
                    temp_latency =  current_timestamp - (FpuPipe[i].end_timestamp+1) 
                                    + (insn_latency - past_latency);
                }
                else
                {
                    temp_latency =  current_timestamp - (FpuPipe[i].end_timestamp+1);
                }
            }
            else
            {
                temp_latency = insn_latency;
            }

            if (temp_latency < latency)
            {
                latency = temp_latency;
            }
        }

        return latency;
    }

    void enqueue(FpuOp value) 
    {
        // Find the entry with the smallest timestamp and replace it with the new one
        int index = 0;
        int end_timestamp = FpuPipe[0].end_timestamp;

        for (int i=0; i<2; i++)
        {
            if (FpuPipe[i].end_timestamp < end_timestamp)
            {
                index = i;
            }
        }

        FpuPipe[index] = value;
    }
};


// Instruction stored in FIFO with depth of 1 should have latency of 1 cycles originally.
class FIFODepth1
{
public:

    FpuOp FpuPipe[1];

    bool check_dependency(int index, int rs1, int rs2, int rs3)
    {
        bool dependency = false;
        if (FpuPipe[index].rd >= 0)
        {
            dependency |= (rs1 == FpuPipe[index].rd);
            dependency |= (rs2 == FpuPipe[index].rd);
            dependency |= (rs3 == FpuPipe[index].rd);
        }
        return dependency;
    }

    int get_latency(int current_timestamp, int insn_latency, int rs1, int rs2, int rs3)
    {
        return insn_latency;
    }

    void enqueue(FpuOp value) 
    {
        // Find the entry with the smallest timestamp and replace it with the new one
        int index = 0;
        int end_timestamp = FpuPipe[0].end_timestamp;

        for (int i=0; i<1; i++)
        {
            if (FpuPipe[i].end_timestamp < end_timestamp)
            {
                index = i;
            }
        }

        FpuPipe[index] = value;
    }
};
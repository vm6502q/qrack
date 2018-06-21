//////////////////////////////////////////////////////////////////////////////////////
//
// (C) Daniel Strano and the Qrack contributors 2017, 2018. All rights reserved.
//
// This is a multithreaded, universal quantum register simulation, allowing
// (nonphysical) register cloning and direct measurement of probability and
// phase, to leverage what advantages classical emulation of qubits can have.
//
// Licensed under the GNU General Public License V3.
// See LICENSE.md in the project root or https://www.gnu.org/licenses/gpl-3.0.en.html
// for details.

#pragma once

#include "qengine_opencl.hpp"
#include "common/parallel_for.hpp"

namespace Qrack {
    
class QEngineOCLMulti;
typedef std::shared_ptr<QEngineOCLMulti> QEngineOCLMultiPtr;

/** OpenCL enhanced QEngineCPU implementation. */
class QEngineOCLMulti : public QInterface, public ParallelFor {
protected:
    real1 runningNorm;
    bitLenInt subQubitCount;
    bitCapInt subMaxQPower;
    bitLenInt subEngineCount;
    bitLenInt maxDeviceOrder;
    size_t subBufferSize;
    OCLEngine* clObj;
    std::vector<QEngineOCLPtr> substateEngines;
    std::vector<std::vector<cl::Buffer>> substateBuffers;
    
    uint32_t randomSeed;
    std::shared_ptr<std::default_random_engine> rand_generator;
    std::uniform_real_distribution<real1> rand_distribution;

public:
    QEngineOCLMulti(bitLenInt qBitCount, bitCapInt initState, std::shared_ptr<std::default_random_engine> rgp = nullptr, int deviceCount = -1);
    
    virtual void SetQubitCount(bitLenInt qb)
    {
        qubitCount = qb;
        maxQPower = 1 << qubitCount;
        subEngineCount = substateEngines.size();
        subQubitCount = qubitCount - log2(subEngineCount);
        subMaxQPower = 1 << subQubitCount;
        subBufferSize = sizeof(complex) * subMaxQPower >> 1;
    }
    
    virtual void SetQuantumState(complex* inputState);
    virtual void SetPermutation(bitCapInt perm);

    virtual bitLenInt Cohere(QEngineOCLMultiPtr toCopy);
    virtual bitLenInt Cohere(QInterfacePtr toCopy) { return Cohere(std::dynamic_pointer_cast<QEngineOCLMulti>(toCopy)); }
    virtual std::map<QInterfacePtr, bitLenInt> Cohere(std::vector<QEngineOCLMultiPtr> toCopy);
    virtual std::map<QInterfacePtr, bitLenInt> Cohere(std::vector<QInterfacePtr> toCopy) {
        std::vector<QEngineOCLMultiPtr> toCpy(toCopy.size());
        for (bitLenInt i = 0; i < (toCopy.size()); i++) {
            toCpy[i] = std::dynamic_pointer_cast<QEngineOCLMulti>(toCopy[i]);
        }
        return Cohere(toCpy);
    }
    virtual void Decohere(bitLenInt start, bitLenInt length, QEngineOCLMultiPtr dest);
    virtual void Decohere(bitLenInt start, bitLenInt length, QInterfacePtr dest) { Decohere(start, length, std::dynamic_pointer_cast<QEngineOCLMulti>(dest)); }
    virtual void Dispose(bitLenInt start, bitLenInt length);

    virtual void CCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target);
    virtual void AntiCCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target);
    virtual void CNOT(bitLenInt control, bitLenInt target);
    virtual void AntiCNOT(bitLenInt control, bitLenInt target);

    virtual void H(bitLenInt qubitIndex);
    virtual bool M(bitLenInt qubitIndex);
    virtual void X(bitLenInt qubitIndex);
    virtual void Y(bitLenInt qubitIndex);
    virtual void Z(bitLenInt qubitIndex);
    virtual void CY(bitLenInt control, bitLenInt target);
    virtual void CZ(bitLenInt control, bitLenInt target);

    virtual void RT(real1 radians, bitLenInt qubitIndex);
    virtual void RX(real1 radians, bitLenInt qubitIndex);
    virtual void CRX(real1 radians, bitLenInt control, bitLenInt target);
    virtual void RY(real1 radians, bitLenInt qubitIndex);
    virtual void CRY(real1 radians, bitLenInt control, bitLenInt target);
    virtual void RZ(real1 radians, bitLenInt qubitIndex);
    virtual void CRZ(real1 radians, bitLenInt control, bitLenInt target);
    virtual void CRT(real1 radians, bitLenInt control, bitLenInt target);
    
    virtual void INC(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void INCC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void INCS(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex);
    virtual void INCSC(
                       bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex);
    virtual void INCSC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void INCBCD(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void INCBCDC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DEC(bitCapInt toSub, bitLenInt start, bitLenInt length);
    virtual void DECC(bitCapInt toSub, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DECS(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex);
    virtual void DECSC(
                       bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt overflowIndex, bitLenInt carryIndex);
    virtual void DECSC(bitCapInt toAdd, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    virtual void DECBCD(bitCapInt toAdd, bitLenInt start, bitLenInt length);
    virtual void DECBCDC(bitCapInt toSub, bitLenInt start, bitLenInt length, bitLenInt carryIndex);
    
    virtual void ZeroPhaseFlip(bitLenInt start, bitLenInt length);
    virtual void CPhaseFlipIfLess(bitCapInt greaterPerm, bitLenInt start, bitLenInt length, bitLenInt flagIndex);
    virtual void PhaseFlip();
    
    virtual bitCapInt IndexedLDA(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
                                 bitLenInt valueLength, unsigned char* values);
    
    virtual bitCapInt IndexedADC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
                                 bitLenInt valueLength, bitLenInt carryIndex, unsigned char* values);
    virtual bitCapInt IndexedSBC(bitLenInt indexStart, bitLenInt indexLength, bitLenInt valueStart,
                                 bitLenInt valueLength, bitLenInt carryIndex, unsigned char* values);
    
    virtual void Swap(bitLenInt qubitIndex1, bitLenInt qubitIndex2);
    virtual void CopyState(QInterfacePtr orig) { CopyState(std::dynamic_pointer_cast<QEngineOCLMulti>(orig)); }
    virtual void CopyState(QEngineOCLMultiPtr orig);
    virtual real1 Prob(bitLenInt qubitIndex);
    virtual real1 ProbAll(bitCapInt fullRegister);
    
    virtual void X(bitLenInt start, bitLenInt length);
    virtual void CNOT(bitLenInt control, bitLenInt target, bitLenInt length);
    virtual void AntiCNOT(bitLenInt control, bitLenInt target, bitLenInt length);
    virtual void CCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target, bitLenInt length);
    virtual void AntiCCNOT(bitLenInt control1, bitLenInt control2, bitLenInt target, bitLenInt length);
    //virtual void AND(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    //virtual void OR(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    //virtual void XOR(bitLenInt inputBit1, bitLenInt inputBit2, bitLenInt outputBit, bitLenInt length);
    
protected:
    typedef void (QEngineOCL::*GFn)(bitLenInt);
    typedef void (QEngineOCL::*RGFn)(real1, bitLenInt);
    typedef void (QEngineOCL::*CGFn)(bitLenInt, bitLenInt);
    typedef void (QEngineOCL::*CRGFn)(real1, bitLenInt, bitLenInt);
    typedef void (QEngineOCL::*CCGFn)(bitLenInt, bitLenInt, bitLenInt);
    template<typename F, typename ... Args> void SingleBitGate(bool doNormalize, bitLenInt bit, F fn, Args ... gfnArgs);
    template<typename CF, typename F, typename ... Args> void ControlledGate(bool anti, bitLenInt controlBit, bitLenInt targetBit, CF cfn, F fn, Args ... gfnArgs);
    template<typename CCF, typename CF, typename F, typename ... Args> void DoublyControlledGate(bool anti, bitLenInt controlBit1, bitLenInt controlBit2, bitLenInt targetBit, CCF ccfn, CF cfn, F fn, Args ... gfnArgs);
    template<typename CF, typename F, typename ... Args> void ControlledBody(bool anti, bitLenInt controlDepth, bitLenInt controlBit, bitLenInt targetBit, CF cfn, F fn, Args ... gfnArgs);
    
    template <typename F, typename OF> void RegOp(F fn, OF ofn, bitLenInt start, bitLenInt length);
    template <typename F, typename OF> void ControlledRegOp(F fn, OF ofn, bitLenInt control, bitLenInt target, bitLenInt length);
    template <typename F, typename OF> void DoublyControlledRegOp(F fn, OF ofn, bitLenInt control1, bitLenInt control2, bitLenInt target, bitLenInt length);
    
    // For scalable cluster distribution, these methods should ultimately be entirely removed:
    void CombineAllEngines();
    void SeparateAllEngines();
    template <typename F> void CombineAndOp(F fn, std::vector<bitLenInt> bits);
    
    void NormalizeState();
    
private:
    void ShuffleBuffers(CommandQueuePtr queue, cl::Buffer buff1, cl::Buffer buff2, cl::Buffer tempBuffer);
    void SwapBuffersLow(CommandQueuePtr queue, cl::Buffer buff1, cl::Buffer buff2, cl::Buffer tempBuffer);
    
    inline bitCapInt log2(bitCapInt n) {
        bitLenInt pow = 0;
        bitLenInt p = n >> 1;
        while (p != 0) {
            p >>= 1;
            pow++;
        }
        return pow;
    }
};
} // namespace Qrack
